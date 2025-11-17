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

namespace taskhub {
    void SystemHandler::health(const httplib::Request &, httplib::Response &res) {
        Logger::info("GET /api/health");

        nlohmann::json data;
        data["status"] = "healthy";
        auto now = std::chrono::system_clock::now();
        auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        std::time_t tt = std::chrono::system_clock::to_time_t(now);
        std::ostringstream time_stream;
        time_stream << std::put_time(std::localtime(&tt), "%Y-%m-%d %H:%M:%S")
                    << '.' << std::setw(3) << std::setfill('0') << ms.count();
        data["time"] = time_stream.str();
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
