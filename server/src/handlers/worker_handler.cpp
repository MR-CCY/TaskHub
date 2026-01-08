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
                if (p.first == "worker_id") {
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

    // Request: GET /api/workers/proxy/logs?worker_id=...&task_id=...&run_id=...&from=...&limit=...
    // Response: 转发 /api/tasks/logs 返回内容
    void WorkHandler::workers_proxy_logs(const httplib::Request &req, httplib::Response &res)
    {
        if (!req.has_param("worker_id") || !req.has_param("task_id")) {
            resp::bad_request(res, "missing worker_id/task_id");
            return;
        }

        const std::string workerId = req.get_param_value("worker_id");
        const std::string taskId = req.get_param_value("task_id");
        const std::string runId = req.has_param("run_id") ? req.get_param_value("run_id") : "";
        const std::string from = req.has_param("from") ? req.get_param_value("from") : "";
        const std::string limit = req.has_param("limit") ? req.get_param_value("limit") : "";

        auto optWorker = find_worker_by_id(workerId);
        if (!optWorker.has_value()) {
            resp::not_found(res, "worker not found", 404);
            return;
        }
        const auto& worker = *optWorker;
        if (!worker.isAlive()) {
            resp::error(res, 503, "worker not alive", 503);
            return;
        }

        std::string path = "/api/tasks/logs?task_id=" + taskId;
        if (!runId.empty()) {
            path += "&run_id=" + runId;
        }
        if (!from.empty()) {
            path += "&from=" + from;
        }
        if (!limit.empty()) {
            path += "&limit=" + limit;
        }

        httplib::Client cli(worker.host, worker.port);
        cli.set_connection_timeout(2, 0);
        cli.set_read_timeout(10, 0);
        cli.set_write_timeout(5, 0);

        auto workerResp = cli.Get(path.c_str());
        if (!workerResp) {
            resp::error(res, 502, "worker log query failed: no response", 502);
            return;
        }
        if (workerResp->status >= 300) {
            resp::error(res, 502, "worker log query failed: " + std::to_string(workerResp->status), 502);
            return;
        }

        res.status = workerResp->status;
        res.set_content(workerResp->body, "application/json; charset=utf-8");
    }

    // Request: GET /api/workers/proxy/dag/task_runs?worker_id=...&run_id=...&name=...&limit=...
    // Response: 转发 /api/dag/task_runs 返回内容
    void WorkHandler::workers_proxy_task_runs(const httplib::Request &req, httplib::Response &res)
    {
        if (!req.has_param("worker_id")) {
            resp::bad_request(res, "missing worker_id");
            return;
        }
        const std::string workerId = req.get_param_value("worker_id");
        auto optWorker = find_worker_by_id(workerId);
        if (!optWorker.has_value()) {
            resp::not_found(res, "worker not found", 404);
            return;
        }
        const auto& worker = *optWorker;
        if (!worker.isAlive()) {
            resp::error(res, 503, "worker not alive", 503);
            return;
        }

        std::string path = "/api/dag/task_runs";
        const std::string query = build_query_without_worker_id(req);
        if (!query.empty()) {
            path += "?";
            path += query;
        }

        httplib::Client cli(worker.host, worker.port);
        cli.set_connection_timeout(2, 0);
        cli.set_read_timeout(10, 0);
        cli.set_write_timeout(5, 0);

        auto workerResp = cli.Get(path.c_str());
        if (!workerResp) {
            resp::error(res, 502, "worker task_runs query failed: no response", 502);
            return;
        }
        if (workerResp->status >= 300) {
            resp::error(res, 502, "worker task_runs query failed: " + std::to_string(workerResp->status), 502);
            return;
        }
        res.status = workerResp->status;
        res.set_content(workerResp->body, "application/json; charset=utf-8");
    }

    // Request: GET /api/workers/proxy/dag/events?worker_id=...&run_id=...&task_id=...&type=...&event=...&start_ts_ms=...&end_ts_ms=...&limit=...
    // Response: 转发 /api/dag/events 返回内容
    void WorkHandler::workers_proxy_task_events(const httplib::Request &req, httplib::Response &res)
    {
        if (!req.has_param("worker_id")) {
            resp::bad_request(res, "missing worker_id");
            return;
        }
        const std::string workerId = req.get_param_value("worker_id");
        auto optWorker = find_worker_by_id(workerId);
        if (!optWorker.has_value()) {
            resp::not_found(res, "worker not found", 404);
            return;
        }
        const auto& worker = *optWorker;
        if (!worker.isAlive()) {
            resp::error(res, 503, "worker not alive", 503);
            return;
        }

        std::string path = "/api/dag/events";
        const std::string query = build_query_without_worker_id(req);
        if (!query.empty()) {
            path += "?";
            path += query;
        }

        httplib::Client cli(worker.host, worker.port);
        cli.set_connection_timeout(2, 0);
        cli.set_read_timeout(10, 0);
        cli.set_write_timeout(5, 0);

        auto workerResp = cli.Get(path.c_str());
        if (!workerResp) {
            resp::error(res, 502, "worker events query failed: no response", 502);
            return;
        }
        if (workerResp->status >= 300) {
            resp::error(res, 502, "worker events query failed: " + std::to_string(workerResp->status), 502);
            return;
        }
        res.status = workerResp->status;
        res.set_content(workerResp->body, "application/json; charset=utf-8");
    }
    // Request: POST /api/worker/execute
    //   Body JSON: TaskConfig（同 Dag/Runner），exec_type 不能为 Remote
    // Response:
    //   200 {status:int,message,exit_code,duration_ms,stdout,stderr,attempt,max_attempts,metadata:{...}}
    //   400 {"code":400,"message":"invalid json"|"worker cannot execute Remote task","data":null}; 500 {"code":500,"message":"internal error: ...","data":null}
    void WorkHandler::worker_execute(const httplib::Request &req, httplib::Response &res)
    {
        Logger::info("Worker execute task request: " + req.body);
        try {
            json jReq = json::parse(req.body, nullptr, false);
            if (jReq.is_discarded()) {
                resp::error(res, 400, "invalid json", 400);
                return;
            }
            const std::string payloadType = normalize_payload_type(jReq);
            const json& payload = extract_payload(jReq);

            if (payloadType == "dag") {
                if (!payload.is_object()) {
                    resp::bad_request(res, "dag payload must be object", 400);
                    return;
                }
                json dagBody = payload;
                if (!dagBody.contains("tasks") || !dagBody["tasks"].is_array()) {
                    resp::bad_request(res, R"({"error":"tasks must be array"})", 400);
                    return;
                }

                std::vector<json> taskList;
                std::string err;
                if (!build_task_list_from_tasks_array(dagBody["tasks"], taskList, err)) {
                    resp::bad_request(res, err, 400);
                    return;
                }

                std::string runId = dagBody.value("run_id", std::string{});
                if (runId.empty()) {
                    runId = std::to_string(utils::now_millis()) + "_" + utils::random_string(6);
                }

                dagrun::injectRunId(dagBody, runId);
                dagrun::persistRunAndTasks(runId, dagBody, "worker_execute");
                Logger::info("Worker execute dag: run_id=" + runId +
                             ", tasks=" + std::to_string(taskList.size()));

                dag::DagThreadPool::instance().post(
                    [dagBody = std::move(dagBody), runId]() mutable {
                        try {
                            dag::DagService::instance().runDag(dagBody, runId);
                        } catch (const std::exception& e) {
                            Logger::error(std::string("worker_execute dag thread exception: ") + e.what());
                        }
                    }, core::TaskPriority::Critical);

                json data;
                data["type"] = "dag";
                data["ok"] = true;
                data["run_id"] = runId;
                data["task_ids"] = taskList;
                resp::ok(res, data);
                return;
            }

            if (payloadType == "template") {
                if (!payload.is_object()) {
                    resp::bad_request(res, "template payload must be object", 400);
                    return;
                }
                const std::string templateId = payload.value("template_id", std::string{});
                json params = payload.value("params", json::object());
                if (templateId.empty() || !params.is_object()) {
                    resp::bad_request(res, R"({"error":"missing required fields"})", 400);
                    return;
                }

                auto& store = tpl::TemplateStore::instance();
                auto tplOpt = store.get(templateId);
                if (!tplOpt) {
                    resp::error(res, 404, R"({"error":"template not found"})", 404);
                    return;
                }

                tpl::ParamMap p;
                for (auto& [k, v] : params.items()) {
                    p[k] = v;
                }
                auto r = tpl::TemplateRenderer::render(*tplOpt, p);
                if (!r.ok) {
                    resp::error(res, 400, r.error, 400);
                    return;
                }

                json rendered = r.rendered;
                if (!rendered.contains("config") || !rendered["config"].is_object()) {
                    rendered["config"] = json::object();
                }
                rendered["config"]["template_id"] = templateId;
                if (!rendered.contains("name")) {
                    rendered["name"] = tplOpt->name;
                }
                rendered["config"]["name"] = rendered.value("name", tplOpt->name);

                std::vector<json> taskList;
                std::string err;
                if (!build_task_list_from_payload(rendered, taskList, err)) {
                    resp::bad_request(res, err, 400);
                    return;
                }

                std::string runId = payload.value("run_id", std::string{});
                if (runId.empty()) {
                    runId = std::to_string(utils::now_millis()) + "_" + utils::random_string(6);
                }

                dagrun::injectRunId(rendered, runId);
                dagrun::persistRunAndTasks(runId, rendered, "template");
                Logger::info("Worker execute template: template_id=" + templateId +
                             ", run_id=" + runId +
                             ", tasks=" + std::to_string(taskList.size()));

                dag::DagThreadPool::instance().post(
                    [rendered = std::move(rendered), runId]() mutable {
                        try {
                            dag::DagService::instance().runDag(rendered, runId);
                        } catch (const std::exception& e) {
                            Logger::error(std::string("worker_execute template thread exception: ") + e.what());
                        }
                    }, core::TaskPriority::Critical);

                json data;
                data["type"] = "template";
                data["ok"] = true;
                data["run_id"] = runId;
                data["template_id"] = templateId;
                data["task_ids"] = taskList;
                resp::ok(res, data);
                return;
            }

            // task (default)
            core::TaskConfig cfg = core::parseTaskConfigFromReq(payload);
            if (cfg.execType == core::TaskExecType::Remote) {
                resp::error(res, 400, "worker cannot execute Remote task", 400);
                return;
            }
            Logger::info("Worker execute task: id=" + cfg.id.value + ", name=" + cfg.name + ", execType=" + core::TaskExecTypetoString(cfg.execType));
            core::TaskResult r = runner::TaskRunner::instance().run(cfg, nullptr);

            json jResp = taskResultToJson(r);
            Logger::info("Worker execute task finished: id=" + cfg.id.value +
                        ", status=" + std::to_string(static_cast<int>(r.status)) +
                        ", durationMs=" +  std::to_string(r.durationMs));
            resp::ok(res, jResp);
        } catch (const std::exception& e) {
            // 兜底：任何未预期异常返回 500，避免线程挂掉
            resp::error(res, 500, std::string("internal error: ") + e.what(), 500);
        } catch (...) {
            resp::error(res, 500, "internal error: unknown", 500);
        }
    }
}
