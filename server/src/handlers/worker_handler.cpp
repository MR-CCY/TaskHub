#include "worker_handler.h"
#include "worker/server_worker_registry.h"
#include "log/logger.h"
#include "json.hpp"
#include "runner/task_config.h"
#include "runner/task_runner.h"
#include "runner/task_result.h"
#include "dag/dag_service.h"
#include "dag/dag_run_utils.h"
#include "dag/dag_thread_pool.h"
#include "template/template_store.h"
#include "template/template_renderer.h"
#include "core/http_response.h"
#include "handlers/log_handler.h"
#include "handlers/task_handler.h"
#include "db/task_run_repo.h"
#include "utils/http_params.h"
#include "utils/utils.h"
#include <chrono>
#include <algorithm>
#include <utility>
#include <optional>
#include <cctype>

using json = nlohmann::json;

namespace taskhub
{ 
    using taskhub::worker::ServerWorkerRegistry;
    using taskhub::worker::WorkerInfo;

    namespace {
        json worker_to_json(const WorkerInfo& w)
        {
            json jw;
            jw["id"] = w.id;
            jw["host"] = w.host;
            jw["port"] = w.port;
            jw["running_tasks"] = w.runningTasks;
            jw["max_running_tasks"] = w.maxRunningTasks;
            jw["full"] = w.isFull();
            jw["alive"] = w.isAlive();

            auto now = std::chrono::steady_clock::now();
            auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - w.lastHeartbeat).count();
            jw["last_seen_ms_ago"] = diff;

            jw["queues"] = json::array();
            for (const auto& q : w.queues) jw["queues"].push_back(q);

            jw["labels"] = json::array();
            for (const auto& lb : w.labels) jw["labels"].push_back(lb);

            return jw;
        }

        std::string normalize_payload_type(const json& jReq)
        {
            if (jReq.contains("type") && jReq["type"].is_string()) {
                std::string t = jReq["type"].get<std::string>();
                for (auto& ch : t) {
                    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                }
                return t;
            }
            return "task";
        }

        const json& extract_payload(const json& jReq)
        {
            if (jReq.contains("payload") && jReq["payload"].is_object()) {
                return jReq["payload"];
            }
            return jReq;
        }

        json build_dag_response(const dag::DagResult& dagResult, const std::string& runId)
        {
            std::vector<std::string> successIds;
            std::vector<std::string> failedIds;
            std::vector<std::string> skippedIds;
            for (const auto& kv : dagResult.taskResults) {
                const auto& id = kv.first;
                auto status = kv.second.status;
                const std::string idStr = id.value;
                switch (status) {
                case core::TaskStatus::Success:
                    successIds.push_back(idStr);
                    break;
                case core::TaskStatus::Failed:
                case core::TaskStatus::Timeout:
                    failedIds.push_back(idStr);
                    break;
                case core::TaskStatus::Skipped:
                    skippedIds.push_back(idStr);
                    break;
                default:
                    break;
                }
            }

            json respJson;
            respJson["ok"]      = dagResult.success;
            respJson["message"] = dagResult.message;
            respJson["nodes"]   = dagResult.to_json();
            if (!runId.empty()) {
                respJson["run_id"] = runId;
            }
            json summary;
            summary["total"]   = dagResult.taskResults.size();
            summary["success"] = successIds;
            summary["failed"]  = failedIds;
            summary["skipped"] = skippedIds;
            respJson["summary"] = summary;
            return respJson;
        }

        bool build_task_list_from_tasks_array(const json& tasks, std::vector<json>& taskList, std::string& err)
        {
            if (!tasks.is_array()) {
                err = "tasks must be array";
                return false;
            }
            for (const auto& jt : tasks) {
                if (!jt.contains("id") || !jt["id"].is_string()) {
                    err = "task id is required";
                    return false;
                }
                taskList.push_back({{"logical", jt["id"]}, {"task_id", jt["id"]}});
            }
            return true;
        }

        bool build_task_list_from_payload(const json& body, std::vector<json>& taskList, std::string& err)
        {
            if (body.contains("tasks") && body["tasks"].is_array()) {
                return build_task_list_from_tasks_array(body["tasks"], taskList, err);
            }
            if (body.contains("task") && body["task"].is_object()) {
                const auto& t = body["task"];
                if (!t.contains("id") || !t["id"].is_string()) {
                    err = "task id is required";
                    return false;
                }
                taskList.push_back({{"logical", t["id"]}, {"task_id", t["id"]}});
                return true;
            }
            if (body.contains("id") && body["id"].is_string()) {
                taskList.push_back({{"logical", body["id"]}, {"task_id", body["id"]}});
                return true;
            }
            err = "task id is required";
            return false;
        }

        std::optional<WorkerInfo> find_worker_by_id(const std::string& workerId)
        {
            auto workers = ServerWorkerRegistry::instance().listWorkers();
            auto it = std::find_if(workers.begin(), workers.end(), [&](const WorkerInfo& w) {
                return w.id == workerId;
            });
            if (it == workers.end()) {
                return std::nullopt;
            }
            return *it;
        }

        std::string build_query_without_worker_id(const httplib::Request& req)
        {
            std::string query;
            for (const auto& p : req.params) {
                if (p.first == "worker_id" || p.first == "remote_path") {
                    continue;
                }
                if (!query.empty()) {
                    query += "&";
                }
                query += p.first;
                query += "=";
                query += p.second;
            }
            return query;
        }

        struct ProxyTarget {
            bool isLocal{false};
            WorkerInfo worker;
            std::string nextRemotePath;
        };

        // 递归/多跳解析逻辑
        std::optional<ProxyTarget> resolve_proxy_target(const std::string& runId, const std::string& remotePath, std::string& error)
        {
            ProxyTarget target;
            if (remotePath.empty()) {
                target.isLocal = true;
                return target;
            }

            // 1. 拆分路径: "A/B/C" -> head="A", tail="B/C"
            std::string head = remotePath;
            std::string tail;
            auto slashPos = remotePath.find('/');
            if (slashPos != std::string::npos) {
                head = remotePath.substr(0, slashPos);
                tail = remotePath.substr(slashPos + 1);
            }

            // 2. 根据 head (logicalId) + runId 查库
            auto taskRun = TaskRunRepo::instance().get(runId, head);
            if (!taskRun) {
                error = "remote logical node not found: " + head + " in run: " + runId;
                return std::nullopt;
            }

            // 3. 拿到 workerId
            std::string workerId = taskRun->workerId;
            if (workerId.empty()) {
                error = "worker_id not found for node: " + head;
                return std::nullopt;
            }

            auto optWorker = find_worker_by_id(workerId);
            if (!optWorker) {
                error = "worker not found or offline: " + workerId;
                return std::nullopt;
            }
            if (!optWorker->isAlive()) {
                error = "worker not alive: " + workerId;
                return std::nullopt;
            }

            target.isLocal = false;
            target.worker = *optWorker;
            target.nextRemotePath = tail;
            return target;
        }

        void proxy_forward(const ProxyTarget& target, const std::string& originalPath, const httplib::Request& req, httplib::Response& res) 
        {
            httplib::Client cli(target.worker.host, target.worker.port);
            cli.set_connection_timeout(2, 0);
            cli.set_read_timeout(10, 0); 
            cli.set_write_timeout(5, 0);

            std::string path = originalPath; // 比如 /api/workers/proxy/logs
            
            // 重构 query param
            // 保留大部分参数，除了 remote_path 需要替换
            std::string query;
            for (const auto& p : req.params) {
                if (p.first == "remote_path" || p.first == "worker_id") continue;
                if (!query.empty()) query += "&";
                query += p.first + "=" + p.second;
            }
            // append next remote_path
             if (!query.empty()) query += "&";
             query += "remote_path=" + target.nextRemotePath;

             // 如果 originalPath 不含 ?，加 ?
             if (path.find('?') == std::string::npos) {
                 path += "?" + query;
             } else {
                 // 理论上 originalPath 是 path base
                 // 这里 originalPath 传入的是 req.path 吗？
                 // 下面调用时传入 req.path
                 path += "?" + query;
             }

            auto workerResp = cli.Get(path.c_str());
            if (!workerResp) {
                resp::error(res, 502, "proxy forward failed: no response", 502);
                return;
            }
            res.status = workerResp->status;
            res.set_content(workerResp->body, "application/json; charset=utf-8");
        }
    }
    
    void WorkHandler::setup_routes(httplib::Server& server)
    {
        // TODO(M11.2): Worker 注册接口
        // POST /api/workers/register
        server.Post("/api/workers/register", &WorkHandler::workers_register);
    
        // TODO(M11.2): Worker 心跳接口
        // POST /api/workers/heartbeat
        server.Post("/api/workers/heartbeat", &WorkHandler::workers_heartbeat);
    
        // TODO(M11.3+):（可选）Worker 列表接口
        // GET /api/workers
        server.Get("/api/workers", &WorkHandler::workers_list);
        server.Get("/api/workers/connected", &WorkHandler::workers_connected);
        server.Get("/api/workers/proxy/logs", &WorkHandler::workers_proxy_logs);
        server.Get("/api/workers/proxy/dag/task_runs", &WorkHandler::workers_proxy_task_runs);
        server.Get("/api/workers/proxy/dag/events", &WorkHandler::workers_proxy_task_events);

        server.Post("/api/worker/execute", &WorkHandler::worker_execute);
    }
    // Request: POST /api/workers/register
    //   Body JSON: {"id":"worker-1","host":"127.0.0.1","port":8083,"queues":["default"],"labels":["gpu"],"running_tasks":0}
    // Response:
    //   200 {"ok":true}
    //   400 {"code":400,"message":"invalid json"|"missing required fields: id/port","data":null}
    //   500 {"code":500,"message":"internal error: ...","data":null}
    void WorkHandler::workers_register(const httplib::Request &req, httplib::Response &res)
    {
        Logger::info("Worker register request: " + req.body);
        try {
            json jReq = json::parse(req.body, nullptr, false);
            if (jReq.is_discarded() || !jReq.is_object()) {
                resp::error(res, 400, "invalid json", 400);
                return;
            }

            WorkerInfo info;
            info.id   = jReq.value("id", std::string{});
            info.host = jReq.value("host", std::string{"127.0.0.1"});
            info.port = jReq.value("port", 0);

            // queues
            if (jReq.contains("queues") && jReq["queues"].is_array()) {
                for (const auto& q : jReq["queues"]) {
                    if (q.is_string()) info.queues.push_back(q.get<std::string>());
                }
            }

            // labels (optional)
            if (jReq.contains("labels") && jReq["labels"].is_array()) {
                for (const auto& lb : jReq["labels"]) {
                    if (lb.is_string()) info.labels.push_back(lb.get<std::string>());
                }
            }

            info.runningTasks  = jReq.value("running_tasks", 0);
            info.maxRunningTasks = jReq.value("max_running_tasks", 1);
            if (info.maxRunningTasks <= 0) {
                info.maxRunningTasks = 1;
            }
            info.lastHeartbeat = std::chrono::steady_clock::now();

            if (info.id.empty() || info.port <= 0) {
                resp::error(res, 400, "missing required fields: id/port", 400);
                return;
            }

            ServerWorkerRegistry::instance().upsertWorker(info);

            Logger::info("Worker registered: id=" + info.id + ", host=" + info.host +
                         ", port=" + std::to_string(info.port) +
                         ", max_running_tasks=" + std::to_string(info.maxRunningTasks));

            json j;
            j["ok"] = true;
            resp::ok(res, j);
        } catch (const std::exception& e) {
            resp::error(res, 500, std::string("internal error: ") + e.what(), 500);
        } catch (...) {
            resp::error(res, 500, "internal error: unknown", 500);
        }
    }
    // Request: POST /api/workers/heartbeat
    //   Body JSON: {"id":"worker-1","running_tasks":2}
    // Response:
    //   200 {"ok":true}
    //   400 {"code":400,"message":"missing worker id","data":null}; 404 {"code":404,"message":"worker not found","data":null}; 500 {"code":500,"message":"internal error","data":null}
    void WorkHandler::workers_heartbeat(const httplib::Request &req, httplib::Response &res)
    {
       Logger::info("Worker heartbeat request: " + req.body);
        try {
            auto j = json::parse(req.body);
    
            std::string id = j.value("id", "");
            int running = j.value("running_tasks", 0);
    
            if (id.empty()) {
                Logger::error("Worker heartbeat request missing worker id");
                resp::error(res, 400, "missing worker id", 400);
                return;
            }
    
            bool ok = ServerWorkerRegistry::instance().touchHeartbeat(id, running);
            if (!ok) {
                Logger::warn("worker heartbeat: worker not found"+id);
                resp::error(res, 404, "worker not found", 404);
                return;
            }
    
            resp::ok(res);
        } catch (...) {
            resp::error(res, 500, "internal error", 500);
        }
    }
    // Request: GET /api/workers
    // Response:
    //   200 {"ok":true,"workers":[{id,host,port,running_tasks,alive,last_seen_ms_ago,queues[],labels[]}]}
    //   500 {"code":500,"message":"internal error: ...","data":null}
    void WorkHandler::workers_list(const httplib::Request &req, httplib::Response &res)
    {
        Logger::info("Worker list request");
        try {
            auto workers = ServerWorkerRegistry::instance().listWorkers();

            json arr = json::array();
            for (const auto& w : workers) {
                arr.push_back(worker_to_json(w));
            }

            json j;
            j["ok"] = true;
            j["workers"] = arr;
            Logger::info("Worker List: id="+ arr.dump());
            resp::ok(res, j);
        } catch (const std::exception& e) {
            resp::error(res, 500, std::string("internal error: ") + e.what(), 500);
        } catch (...) {
            resp::error(res, 500, "internal error: unknown", 500);
        }
    }

    // Request: GET /api/workers/connected
    // Response:
    //   200 {"ok":true,"workers":[{...}]}（仅 alive=true）
    void WorkHandler::workers_connected(const httplib::Request &req, httplib::Response &res)
    {
        Logger::info("Worker connected list request");
        try {
            auto workers = ServerWorkerRegistry::instance().listWorkers();

            json arr = json::array();
            for (const auto& w : workers) {
                if (!w.isAlive()) {
                    continue;
                }
                arr.push_back(worker_to_json(w));
            }

            json j;
            j["ok"] = true;
            j["workers"] = arr;

            resp::ok(res, j);
        } catch (const std::exception& e) {
            resp::error(res, 500, std::string("internal error: ") + e.what(), 500);
        } catch (...) {
            resp::error(res, 500, "internal error: unknown", 500);
        }
    }

    // Request: GET /api/workers/proxy/logs?remote_path=...&run_id=...&task_id=...
    // Response: 转发 or 本地查询
    void WorkHandler::workers_proxy_logs(const httplib::Request &req, httplib::Response &res)
    {
        std::string remotePath = req.get_param_value("remote_path");
        std::string runId = req.get_param_value("run_id");

        // 兼容旧逻辑：如果传了 worker_id 且没传 remote_path，走旧的直连转发
        if (req.has_param("worker_id") && remotePath.empty()) {
             const std::string workerId = req.get_param_value("worker_id");
             const std::string taskId = req.get_param_value("task_id");
             auto optWorker = find_worker_by_id(workerId);
             if (!optWorker.has_value() || !optWorker->isAlive()) {
                 resp::error(res, 404, "worker not found or dead", 404);
                 return;
             }
             ProxyTarget t;
             t.isLocal = false;
             t.worker = *optWorker;
             t.nextRemotePath = ""; // 旧逻辑不用 path
             proxy_forward(t, "/api/tasks/logs", req, res);
             return;
        }

        std::string err;
        auto target = resolve_proxy_target(runId, remotePath, err);
        if (!target) {
            resp::error(res, 404, err, 404);
            return;
        }

        if (target->isLocal) {
            // 本地执行：直接调用 LogHandler
            // query: task_id, run_id, from, limit
            LogHandler::query(req, res);
        } else {
            // 转发
            proxy_forward(*target, "/api/workers/proxy/logs", req, res);
        }
    }

    // Request: GET /api/workers/proxy/dag/task_runs?remote_path=...&run_id=...
    void WorkHandler::workers_proxy_task_runs(const httplib::Request &req, httplib::Response &res)
    {
        std::string remotePath = req.get_param_value("remote_path");
        std::string runId = req.get_param_value("run_id");

        std::string err;
        auto target = resolve_proxy_target(runId, remotePath, err);
        if (!target) {
            resp::error(res, 404, err, 404);
            return;
        }

        if (target->isLocal) {
            TaskHandler::queryTaskRuns(req, res);
        } else {
            proxy_forward(*target, "/api/workers/proxy/dag/task_runs", req, res);
        }
    }

    // Request: GET /api/workers/proxy/dag/events
    void WorkHandler::workers_proxy_task_events(const httplib::Request &req, httplib::Response &res)
    {
        std::string remotePath = req.get_param_value("remote_path");
        std::string runId = req.get_param_value("run_id");

        std::string err;
        auto target = resolve_proxy_target(runId, remotePath, err);
        if (!target) {
            resp::error(res, 404, err, 404);
            return;
        }

        if (target->isLocal) {
            TaskHandler::queryTaskEvents(req, res);
        } else {
            proxy_forward(*target, "/api/workers/proxy/dag/events", req, res);
        }
    }
    // Request: POST /api/worker/execute
    //   Body JSON: {"type":"dag","payload":{...}}  （当前用于 Remote 节点的 DAG 异步派发）
    // Response:
    //   200 {"type":"dag","ok":true,"run_id":"...","task_ids":[{logical,task_id},...]}
    //   400 {"code":400,"message":"invalid json"|"...","data":null}; 500 {"code":500,"message":"internal error: ...","data":null}
    void WorkHandler::worker_execute(const httplib::Request &req, httplib::Response &res)
    {
        Logger::info("Worker execute task request: " + req.body);

        json body;
        try {
            body = json::parse(req.body);
        } catch (const std::exception& e) {
            resp::bad_request(res, "Invalid JSON", 1002);
            return;
        }

        // 统一由 DagService 生成 ID 和处理持久化
        std::string runId = body.value("run_id", std::string{});
        if (runId.empty()) {
            runId = std::to_string(utils::now_millis()) + "_" + utils::random_string(6);
        }
        
        dag::DagThreadPool::instance().post([body = std::move(body), runId]() mutable {
            try {
                dag::DagService::instance().runDag(body, "worker", runId);
            } catch (const std::exception& e) {
                Logger::error(std::string("worker thread exception: ") + e.what());
            }
        },core::TaskPriority::Critical);

        json data;
        data["run_id"] = runId;
        resp::ok(res, data);
    }
}
