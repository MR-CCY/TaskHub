//
// Created by 曹宸源 on 2025/11/13.
//

#include "task_handler.h"
#include "core/task_manager.h"
#include "httplib.h"
#include "json.hpp"
#include "core/logger.h"
#include "core/task_runner.h"
#include "core/auth_manager.h"
#include "core/ws_task_events.h"
#include "core/http_response.h"
#include "core/worker_pool.h"
#include "dag/dag_builder.h"
#include "dag/dag_executor.h"
#include "runner/taskRunner.h"
// using taskhub::resp;
// using taskhub::broadcast_task_event;
namespace taskhub {

    void TaskHandler::create(const httplib::Request &req, httplib::Response &res)
    {
        auto user_opt = AuthManager::instance().user_from_request(req);
        if (!user_opt) {
            nlohmann::json resp;
            resp["code"] = 401;
            resp["message"] = "unauthorized";
            resp["data"] = nullptr;
    
            res.status = 401;
            res.set_content(resp.dump(), "application/json");
            return;
        }
    
        Logger::info("GET /api/tasks");

        nlohmann::json req_json;
        try {       
            req_json = nlohmann::json::parse(req.body);
            std::string name = req_json.value("name", "");
            if(name.empty()){
                nlohmann::json resp;
                resp["code"]    = 1001;
                resp["message"] = "Missing 'name' field";
                res.status = 400;
                res.set_content(resp.dump(), "application/json");
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
            nlohmann::json resp;
            resp["code"]    = 1002;
            resp["message"] = "Invalid JSON";
            res.status = 400;
            res.set_content(resp.dump(), "application/json");
            return;
        }
    }

    void TaskHandler::list(const httplib::Request &req, httplib::Response &res)
    {
        auto user_opt = AuthManager::instance().user_from_request(req);
        if (!user_opt) {
            nlohmann::json resp;
            resp["code"] = 401;
            resp["message"] = "unauthorized";
            resp["data"] = nullptr;
    
            res.status = 401;
            res.set_content(resp.dump(), "application/json");
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

        res.status = 200;
        res.set_content(resp.dump(), "application/json");
    }

    void TaskHandler::detail(const httplib::Request &req, httplib::Response &res)
    {
        auto user_opt = AuthManager::instance().user_from_request(req);
        if (!user_opt) {
            nlohmann::json resp;
            resp["code"] = 401;
            resp["message"] = "unauthorized";
            resp["data"] = nullptr;
    
            res.status = 401;
            res.set_content(resp.dump(), "application/json");
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
            nlohmann::json resp;
            resp["code"]    = 1003;
            resp["message"] = "Task not found";
            res.status = 404;
            res.set_content(resp.dump(), "application/json");
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
        res.status = 200;
        res.set_content(resp.dump(), "application/json");
    }

    void TaskHandler::cancel_task(const httplib::Request &req, httplib::Response &res)
    {
        auto user_opt = AuthManager::instance().user_from_request(req);
        if (!user_opt) {
            nlohmann::json resp;
            resp["code"] = 401;
            resp["message"] = "unauthorized";
            resp["data"] = nullptr;
    
            res.status = 401;
            res.set_content(resp.dump(), "application/json");
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
            // auto& dagSvc = api::DagService::instance();

            // 3. 解析 config
            dag::DagConfig cfg;
            if (body.contains("config")) {
                auto jcfg = body["config"];
                std::string policy = jcfg.value("fail_policy", "SkipDownstream");
                if (policy == "FailFast") {
                    cfg.failPolicy = dag::FailPolicy::FailFast;
                } else {
                    cfg.failPolicy = dag::FailPolicy::SkipDownstream;
                }
                cfg.maxParallel = jcfg.value("max_parallel", 4u);
            }
            // 4. 解析任务列表，构造 DagTaskSpec + TaskConfig
            std::vector<dag::DagTaskSpec> specs;
            if (!body.contains("tasks") || !body["tasks"].is_array()) {
                // 返回 400
                resp::error(res, 400, R"({"ok":false,"error":"tasks must be array"})");
                return;
            }

            for (auto& jtask : body["tasks"]) {
                dag::DagTaskSpec spec;
                core::TaskConfig cfgTask;
                cfgTask= core::parseTaskConfigFromReq(jtask);
                spec.id = cfgTask.id;
                spec.runnerCfg = cfgTask;

                if (jtask.contains("deps")) {
                    for (auto& d : jtask["deps"]) {
                        spec.deps.push_back(core::TaskId{ d.get<std::string>() });
                    }
                }
                
                specs.push_back(std::move(spec));
            }

            // 5. 准备 callbacks（这里先简单记录一下每个节点最终状态）
            std::vector<json> nodeStates;
            std::mutex nodeStatesMutex; // 并发访问需要保护

            dag::DagEventCallbacks callbacks;
            callbacks.onNodeStatusChanged =
                [&nodeStates, &nodeStatesMutex](const core::TaskId& id, core::TaskStatus st) {
                    std::lock_guard<std::mutex> lk(nodeStatesMutex);
                    json j;
                    j["id"] = id.value;
                    j["status"] = static_cast<int>(st);
                    nodeStates.push_back(std::move(j));
                };
            callbacks.onDagFinished = [](bool success) {
                // 需要的话，这里可以打日志或发 ws 事件
                Logger::info("dag finished, success: {}");
            };
            // 6. 调用 DagService
                        
            //Step 1：构建 builder，添加所有任务
            dag::DagBuilder builder;
            for (const auto& spec : specs) {
                builder.addTask(spec);
            }

            // Step 2：validats是是否成图，是否有环
            auto validateResult = builder.validate();
            if (!validateResult.ok) {
                resp::error(res, 4001, validateResult.errorMessage);
                Logger::error(validateResult.errorMessage);
                return;
            }

            // TODO Step 3：构建图 + 运行
            auto graph = builder.build();
            dag::DagRunContext ctx(cfg, std::move(graph), callbacks);
            dag::DagExecutor executor(runner::TaskRunner::instance());
            core::TaskResult dagResult=executor.execute(ctx);
            const auto& finalMap = ctx.finalStatus();

            std::vector<std::string> successIds;
            std::vector<std::string> failedIds;
            std::vector<std::string> skippedIds;

            for (const auto& kv : finalMap) {
                const auto& id = kv.first;
                auto status    = kv.second;
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

            // 7. 组装响应 JSON
            json respJson;
            respJson["ok"]      = dagResult.ok();
            respJson["status"]  = static_cast<int>(dagResult.status);
            respJson["message"] = dagResult.message;
            respJson["nodes"]   = nodeStates;
            
            json summary;
            summary["total"]   = finalMap.size();
            summary["success"] = successIds;
            summary["failed"]  = failedIds;
            summary["skipped"] = skippedIds;
            respJson["summary"] = summary;
            Logger::info("==========");
            res.set_content(respJson.dump(), "application/json");
            Logger::info("=222222==");
    
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
