//
// Created by 曹宸源 on 2025/11/13.
//

#include "system_handler.h"
#include "httplib.h"
#include "json.hpp"
#include <chrono>
#include "core/logger.h"
#include <iomanip>
#include <sstream>
#include "core/utils.h"
namespace taskhub {
    void SystemHandler::health(const httplib::Request &, httplib::Response &res) {
        Logger::info("GET /api/health");

        nlohmann::json data;
        data["status"] = "healthy";

        data["time"] = utils::now_string();
        data["uptime_sec"] = 1; // TODO: Replace with actual uptime calculation

        nlohmann::json resp;
        resp["code"]    = 0;
        resp["message"] = "ok";
        resp["data"]    = data;

        res.status = 200;
        res.set_content(resp.dump(), "application/json");
    }

    void SystemHandler::info(const httplib::Request &, httplib::Response &res) {
        Logger::info("GET /api/info");

        nlohmann::json data;
        data["name"]       = "TaskHub Server";
        data["version"]    = "0.1.0";
        data["build_time"] = __DATE__ " " __TIME__;
        data["commit"]     = "dev-local";

        nlohmann::json resp;
        resp["code"]    = 0;
        resp["message"] = "ok";
        resp["data"]    = data;

        res.status = 200;
        res.set_content(resp.dump(), "application/json");
    }
}
