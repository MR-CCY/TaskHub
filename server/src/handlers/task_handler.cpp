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

namespace taskhub {
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
    }

    // Request: POST /api/tasks
    //   Headers: Content-Type: application/json, Auth token header used by AuthManager
    //   Body: {"name":"task1","type":0,"params":{"cmd":"echo hi"},"max_retries":0,"timeout_sec":0}
    // Response: {"code":0,"message":"ok","data":{"id":<taskId>}} on success;
    //           {"code":1001,"message":"Missing 'name' field"} for missing required fields;
    //           {"code":1002,"message":"Invalid JSON"} for bad JSON;
    //           {"code":401,"message":"unauthorized"} when token invalid.
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

    // Request: GET /api/tasks (Auth required)
    // Query: none
    // Response: {"code":0,"message":"ok","data":[{task objects...}]}
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

    // Request: GET /api/tasks/{id} (Auth required)
    // Response: {"code":0,"message":"ok","data":{task fields...}}; 404 if not found; 401 if unauthorized.
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

    // Request: POST /api/tasks/{id}/cancel (Auth required)
    // Body: empty
    // Response: {"code":0,"message":"ok","data":null} on success;
    //           {"code":1003,"message":"Task not found"} if id missing;
    //           {"code":1004,"message":"Task already finished"} if terminal state.
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
   //   Body: {"config":{"fail_policy":"SkipDownstream","max_parallel":4},"tasks":[{"id":"a","exec_type":0,...,"deps":["b"]},...]}
   // Response: {"ok":true/false,"status":<int TaskStatus>,"message":string,"nodes":[{"id":"a","status":int},...],
   //            "summary":{"total":N,"success":[ids],"failed":[ids],"skipped":[ids]}}
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
            auto dagResult= dag::DagService::instance().runDag(body);
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
}
 
