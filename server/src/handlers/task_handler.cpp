//
// Created by 曹宸源 on 2025/11/13.
//

#include "task_handler.h"
#include "core/task_manager.h"
#include "httplib.h"
#include "json.hpp"
#include "core/logger.h"

namespace taskhub {

    void TaskHandler::create(const httplib::Request &req, httplib::Response &res)
    {

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
            data.push_back(t);
        }
        resp["data"] = data;

        res.status = 200;
        res.set_content(resp.dump(), "application/json");
    }

    void TaskHandler::detail(const httplib::Request &req, httplib::Response &res)
    {

        Logger::info("GET R(/api/tasks/(\d+)");
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
        resp["data"] = data;

        res.status = 200;
        res.set_content(resp.dump(), "application/json");
    }

}