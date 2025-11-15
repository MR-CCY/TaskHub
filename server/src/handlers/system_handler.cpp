//
// Created by 曹宸源 on 2025/11/13.
//

#include "system_handler.h"
#include "httplib.h"
#include "json.hpp"
#include "core/logger.h"

namespace taskhub {
    void SystemHandler::health(const httplib::Request &, httplib::Response &res) {
        Logger::info("GET /api/health");

        nlohmann::json data;
        data["status"] = "healthy";
        data["time"]   = "2025-11-14T12:00:00Z";  // TODO: 换成真实时间
        data["uptime_sec"] = 0;                  // TODO: 用启动时间算

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
