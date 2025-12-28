//
// Created by 曹宸源 on 2025/11/13.
//

#include "task_handler.h"
#include "core/task_manager.h"
#include "httplib.h"
#include "json.hpp"
#include "log/logger.h"
#include "core/task_runner.h"
#include "auth/auth_manager.h"
#include "ws/ws_task_events.h"
#include "core/http_response.h"
#include "core/worker_pool.h"
#include "dag/dag_builder.h"
#include "dag/dag_executor.h"
#include "runner/taskRunner.h"
#include "dag/dag_service.h"
#include "dag/dag_thread_pool.h"
#include "db/dag_run_repo.h"
#include "db/task_run_repo.h"
#include "db/task_event_repo.h"
#include "utils/utils.h"
#include <unordered_map>
#include <algorithm>

namespace taskhub {
    using json = nlohmann::json;
    void TaskHandler::setup_routes(httplib::Server &server)
    {
        server.Post("/api/tasks", TaskHandler::create);
        server.Get("/api/tasks",  TaskHandler::list);
        // 正则匹配数字 ID
        server.Get(R"(/api/tasks/(\d+))", TaskHandler::detail);
        //M7
        server.Post(R"(/api/tasks/(\d+)/cancel)", TaskHandler::cancel_task);
        //M8
        server.Post("/api/dag/run", TaskHandler::runDag);
        server.Post("/api/dag/run_async", TaskHandler::runDagAsync);
        server.Get("/api/dag/runs", TaskHandler::queryDagRuns);
        server.Get("/api/dag/task_runs", TaskHandler::queryTaskRuns);
        server.Get("/api/dag/events", TaskHandler::queryTaskEvents);
    }

    // Request: POST /api/tasks （需要 Auth）
    //   Body JSON: {"name":"task1","type":0,"params":{"cmd":"echo hi"},...} 其余字段忽略
    // Response:
    //   200 {"code":0,"message":"ok","data":{"id":<int>}}
    //   400 {"code":1001,"message":"Missing 'name' field","data":null} 或 {"code":1002,"message":"Invalid JSON","data":null}
    //   401 {"code":401,"message":"unauthorized","data":null}
    void TaskHandler::create(const httplib::Request &req, httplib::Response &res)
    {
        auto user_opt = AuthManager::instance().user_from_request(req);
        if (!user_opt) {
            resp::unauthorized(res);
            return;
        }
    
        Logger::info("GET /api/tasks");

        nlohmann::json req_json;
        try {       
            req_json = nlohmann::json::parse(req.body);
            std::string name = req_json.value("name", "");
            if(name.empty()){
                resp::bad_request(res, "Missing 'name' field", 1001);
                return;
            }

            int type = req_json.value("type", 0);
            nlohmann::json params = req_json.value("params", nlohmann::json::object());
            Task::IdType task_id = TaskManager::instance().add_task(name, type, params);  
            
            // ★ 如果有 cmd，就丢到 TaskRunner
            // TaskRunner::instance().enqueue(task_id);
            WorkerPool::instance()->submit(task_id);
            Logger::info("Submitted task id " +  std::to_string(task_id) + " to WorkerPool");
            
            // ★ 广播任务创建事件
            broadcast_task_event("task_created", TaskManager::instance().get_task(task_id).value());

            nlohmann::json resp;
            resp["code"]    = 0;
            resp["message"] = "ok";
            nlohmann::json data;
            data["id"] = task_id;
            resp["data"]    = data;
            res.status = 200;
            res.set_content(resp.dump(), "application/json");
            Logger::info("Created task: " + std::to_string(task_id));
            return;
        } catch (const std::exception& e) {
            Logger::error(std::string("Failed to parse JSON request: ") + e.what());
            resp::bad_request(res, "Invalid JSON", 1002);
            return;
        }
    }

    // Request: GET /api/tasks （需要 Auth）
    // Response: 200 {"code":0,"message":"ok","data":[{id,name,type,status,create_time,update_time,params,exit_code,last_output,last_error,max_retries,retry_count,timeout_sec,cancel_flag}]}; 401 未鉴权
    void TaskHandler::list(const httplib::Request &req, httplib::Response &res)
    {
        auto user_opt = AuthManager::instance().user_from_request(req);
        if (!user_opt) {
            resp::unauthorized(res);
            return;
        }
    
        Logger::info("GET /api/tasks");
        auto tasks = TaskManager::instance().list_tasks();

        nlohmann::json resp;
        resp["code"]    = 0;
        resp["message"] = "ok";
        nlohmann::json data = nlohmann::json::array();
        for (const auto& task : tasks) {
            nlohmann::json t;
            t["id"] = task.id;
            t["name"] = task.name;
            t["type"] = task.type;      
            t["status"] = Task::status_to_string(task.status);
            t["create_time"] = task.create_time;
            t["update_time"] = task.update_time;
            t["params"] = task.params;
            t["exit_code"] = task.exit_code;
            t["last_output"] = task.last_output;
            t["last_error"] = task.last_error;
            t["max_retries"] = task.max_retries;
            t["retry_count"] =task.retry_count;
            t["timeout_sec"] =task.timeout_sec;
            t["cancel_flag"] =task.cancel_flag;
            data.push_back(t);
        }
        resp["data"] = data;

        resp::ok(res, resp["data"]);
    }

    // Request: GET /api/tasks/{id} （需要 Auth）
    // Response:
    //   200 {"code":0,"message":"ok","data":{同 list 单项}}
    //   404 {"code":404,"message":"Task not found","data":null}; 401 未鉴权
    void TaskHandler::detail(const httplib::Request &req, httplib::Response &res)
    {
        auto user_opt = AuthManager::instance().user_from_request(req);
        if (!user_opt) {
            resp::unauthorized(res);
            return;
        }
    
        Logger::info("GET R(/api/tasks/:id)");
        if (req.matches.size() < 2) {
            res.status = 400;
            return;
        }
        Task::IdType task_id = std::stoull(req.matches[1]);
        auto opt_task = TaskManager::instance().get_task(task_id);
        if (!opt_task.has_value()) {
            resp::not_found(res, "Task not found", 1003);
            return;

        }   

        const Task& task = opt_task.value();

        nlohmann::json resp;
        resp["code"]    = 0;
        resp["message"] = "ok";
        nlohmann::json data;
        data["id"] = task.id;
        data["name"] = task.name;
        data["type"] = task.type;      
        data["status"] = Task::status_to_string(task.status);
        data["create_time"] = task.create_time;
        data["update_time"] = task.update_time;
        data["params"] = task.params;
        data["exit_code"] = task.exit_code;
        data["last_output"] = task.last_output;
        data["last_error"] = task.last_error;
        data["max_retries"] = task.max_retries;
        data["retry_count"] =task.retry_count;
        data["timeout_sec"] =task.timeout_sec;
        data["cancel_flag"] =task.cancel_flag;
        resp["data"] = data;
        resp::ok(res, resp["data"]);
    }

    // Request: POST /api/tasks/{id}/cancel （需要 Auth）
    // Response:
    //   200 {"code":0,"message":"ok","data":null}
    //   {"code":1003,"message":"Task not found","data":null}（任务不存在）
    //   {"code":1004,"message":"Task already finished","data":null}（终态不能取消）
    //   401 未鉴权
    void TaskHandler::cancel_task(const httplib::Request &req, httplib::Response &res)
    {
        auto user_opt = AuthManager::instance().user_from_request(req);
        if (!user_opt) {
            resp::unauthorized(res);
            return;
        }
    
        Logger::info("POST /api/cancel_task");

        auto tasks = TaskManager::instance().list_tasks();
        auto id_str = req.matches[1];  // httplib 的路径捕获
        int task_id = std::stoi(id_str);

        auto& tm = TaskManager::instance();
        auto taskPtr = tm.get_task_ptr(task_id);

        if (!taskPtr) {
            resp::error(res, 1003, "Task not found");
            return;
        }

        Task& task = *taskPtr;
        if (task.status == TaskStatus::Success ||
            task.status == TaskStatus::Failed ||
            task.status == TaskStatus::Canceled ||
            task.status == TaskStatus::Timeout) {
            resp::error(res, 1004, "Task already finished");
            return;
        }

        // ★ 设置取消标记
        task.cancel_flag = true;
        task.update_time = utils::now_string();
        tm.updateTask(task);   // 写回缓存和 DB

        broadcast_task_event("task_updated", task);

        resp::ok(res);  // {"code":0,"message":"ok"}

    }
   // Request: POST /api/dag/run
   //   Body JSON:
   //     {"name":"workflow-1","config":{"name":"workflow-1","fail_policy":"SkipDownstream"|"FailFast","max_parallel":4},"tasks":[{"id":"a","name":"step a","exec_type":"Local",...,"deps":["b"]},...]}
   //     （也支持单个 task 对象 {"task":{...}}）
   //   Response:
   //     200 {"ok":true,"message":string,"nodes":[{"id":"a","run_id":"...","result":{status:int,message,exit_code,duration_ms,stdout,stderr,attempt,max_attempts,metadata}}],"run_id":string,"summary":{"total":N,"success":[ids],"failed":[ids],"skipped":[ids]}}
   //     500 {"ok":false,"error":<dag error>} （DagService 失败）
   void TaskHandler::runDag(const httplib::Request &req, httplib::Response &res)
    {
        // auto user_opt = AuthManager::instance().user_from_request(req);
        // if (!user_opt) {
        //     nlohmann::json resp;
        //     resp["code"] = 401;
        //     resp["message"] = "unauthorized";
        //     resp["data"] = nullptr;
    
        //     res.status = 401;
        //     res.set_content(resp.dump(), "application/json");
        //     return;
        // }
        Logger::info("POST /api/run_dag");
        try {
            // 1. 解析请求 JSON
            json body = json::parse(req.body);
            const std::string runId = std::to_string(utils::now_millis()) + "_" + utils::random_string(6);
            auto dagResult= dag::DagService::instance().runDag(body, runId);
            if(!dagResult.success){
                json out;
                out["ok"] = false;
                out["error"] = dagResult.message;
                res.status = 500;
                res.set_content(out.dump(), "application/json");
                return;
            }

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
            respJson["nodes"]= dagResult.to_json();
            respJson["run_id"] = runId;
            json summary;
            summary["total"]   = dagResult.taskResults.size();
            summary["success"] = successIds;
            summary["failed"]  = failedIds;
            summary["skipped"] = skippedIds;
            respJson["summary"] = summary;
            res.set_content(respJson.dump(), "application/json");
        }
        catch (const std::exception& ex) {
            json err;
            err["ok"]    = false;
            err["error"] = ex.what();
            res.status  = 500;
            res.body    = err.dump();
            resp::ok(res); 
        }
    }

    // Request: POST /api/dag/run_async
    //   Body JSON 同 /api/dag/run，必须有 tasks 数组
    // Response:
    //   200 {"code":0,"message":"ok","data":{"run_id":string,"task_ids":[{"logical":<id>,"task_id":<id>},...]}}
    //   400 {"code":1002,"message":"Invalid JSON","data":null} 或 {"code":400,"message":"{\"error\":\"tasks must be array\"}","data":null}
    void TaskHandler::runDagAsync(const httplib::Request &req, httplib::Response &res)
    {
        Logger::info("POST /api/run_dag_async");
        json body;
        try {
            body = json::parse(req.body);
        } catch (const std::exception& e) {
            resp::bad_request(res, "Invalid JSON", 1002);
            return;
        }
        if (!body.contains("tasks") || !body["tasks"].is_array()) {
            resp::bad_request(res, R"({"error":"tasks must be array"})", 400);
            return;
        }

        std::vector<json> taskList;
        for (auto& jt : body["tasks"]) {
            if (!jt.contains("id") || !jt["id"].is_string()) {
                resp::bad_request(res, "task id is required", 400);
                return;
            }
            taskList.push_back({{"logical", jt["id"]}, {"task_id", jt["id"]}});
        }

        const std::string runId = std::to_string(utils::now_millis()) + "_" + utils::random_string(6);
        for (auto& jt : body["tasks"]) {
            jt["run_id"] = runId;
        }

        // 持久化 dag_run 和初始 task_run
        DagRunRepo::instance().insertRun(runId, body, "manual", "");
        for (const auto& jt : body["tasks"]) {
            TaskRunRepo::instance().upsertFromTaskJson(runId, jt);
        }

        // 用 DAG 线程池执行，避免为每个请求创建 detached 线程
        dag::DagThreadPool::instance().post([body = std::move(body), runId]() mutable {
            try {
                dag::DagService::instance().runDag(body, runId);
            } catch (const std::exception& e) {
                Logger::error(std::string("runDagAsync thread exception: ") + e.what());
            }
        });

        json data;
        data["run_id"] = runId;
        data["task_ids"] = taskList;
        resp::ok(res, data);
    }

    static long long parse_ll(const httplib::Request& req, const std::string& key, long long def = 0) {
        if (!req.has_param(key)) return def;
        try {
            return std::stoll(req.get_param_value(key));
        } catch (...) {
            return def;
        }
    }

    static int parse_int(const httplib::Request& req, const std::string& key, int def = 0) {
        if (!req.has_param(key)) return def;
        try {
            return std::stoi(req.get_param_value(key));
        } catch (...) {
            return def;
        }
    }

    void TaskHandler::queryDagRuns(const httplib::Request &req, httplib::Response &resp)
    {
        const std::string runId   = req.has_param("run_id") ? req.get_param_value("run_id") : "";
        const std::string name    = req.has_param("name") ? req.get_param_value("name") : "";
        const long long startTs   = parse_ll(req, "start_ts_ms", 0);
        const long long endTs     = parse_ll(req, "end_ts_ms", 0);
        int limit = parse_int(req, "limit", 100);
        limit = std::clamp(limit, 1, 500);

        auto rows = DagRunRepo::instance().query(runId, name, startTs, endTs, limit);
        json arr = json::array();
        for (const auto& r : rows) {
            json j;
            j["run_id"]        = r.runId;
            j["name"]          = r.name;
            j["source"]        = r.source;
            j["status"]        = r.status;
            j["start_ts_ms"]   = r.startTsMs;
            j["end_ts_ms"]     = r.endTsMs;
            j["total"]         = r.total;
            j["success_count"] = r.successCount;
            j["failed_count"]  = r.failedCount;
            j["skipped_count"] = r.skippedCount;
            j["message"]       = r.message;
            j["workflow_json"] = r.workflowJson;
            j["dag_json"]      = r.dagJson;
            arr.push_back(std::move(j));
        }
        json data;
        data["items"] = std::move(arr);
        resp::ok(resp, data);
    }

    void TaskHandler::queryTaskRuns(const httplib::Request &req, httplib::Response &resp)
    {
        const std::string runId = req.has_param("run_id") ? req.get_param_value("run_id") : "";
        const std::string name  = req.has_param("name") ? req.get_param_value("name") : "";
        int limit = parse_int(req, "limit", 200);
        limit = std::clamp(limit, 1, 1000);

        auto rows = TaskRunRepo::instance().query(runId, name, limit);
        json arr = json::array();
        auto parse_json_safe = [](const std::string& s, const std::string& fallback) {
            try {
                return json::parse(s.empty() ? fallback : s);
            } catch (...) {
                return json::parse(fallback);
            }
        };
        for (const auto& r : rows) {
            json j;
            j["id"]             = r.id;
            j["run_id"]         = r.runId;
            j["logical_id"]     = r.logicalId;
            j["task_id"]        = r.taskId;
            j["name"]           = r.name;
            j["exec_type"]      = r.execType;
            j["exec_command"]   = r.execCommand;
            j["exec_params"]    = parse_json_safe(r.execParamsJson, "{}");
            j["deps"]           = parse_json_safe(r.depsJson, "[]");
            j["status"]         = r.status;
            j["exit_code"]      = r.exitCode;
            j["duration_ms"]    = r.durationMs;
            j["message"]        = r.message;
            j["stdout"]         = r.stdoutText;
            j["stderr"]         = r.stderrText;
            j["attempt"]        = r.attempt;
            j["max_attempts"]   = r.maxAttempts;
            j["start_ts_ms"]    = r.startTsMs;
            j["end_ts_ms"]      = r.endTsMs;
            arr.push_back(std::move(j));
        }
        json data;
        data["items"] = std::move(arr);
        resp::ok(resp, data);
    }

    void TaskHandler::queryTaskEvents(const httplib::Request &req, httplib::Response &resp)
    {
        const std::string runId   = req.has_param("run_id") ? req.get_param_value("run_id") : "";
        const std::string taskId  = req.has_param("task_id") ? req.get_param_value("task_id") : "";
        const std::string type    = req.has_param("type") ? req.get_param_value("type") : "";
        const std::string event   = req.has_param("event") ? req.get_param_value("event") : "";
        const long long startTs   = parse_ll(req, "start_ts_ms", 0);
        const long long endTs     = parse_ll(req, "end_ts_ms", 0);
        int limit = parse_int(req, "limit", 200);
        limit = std::clamp(limit, 1, 1000);

        auto rows = TaskEventRepo::instance().query(runId, taskId, type, event, startTs, endTs, limit);
        json arr = json::array();
        for (const auto& r : rows) {
            json j = r.payload;
            j["id"]      = r.id;
            j["run_id"]  = r.runId;
            j["task_id"] = r.taskId;
            j["type"]    = r.type;
            j["event"]   = r.event;
            j["ts_ms"]   = r.tsMs;
            arr.push_back(std::move(j));
        }
        json data;
        data["items"] = std::move(arr);
        resp::ok(resp, data);
    }
}
 
