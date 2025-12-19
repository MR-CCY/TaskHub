//
// Created by 曹宸源 on 2025/11/13.
//

#include "system_handler.h"
#include "httplib.h"
#include "json.hpp"
#include <chrono>
#include "log/logger.h"
#include <iomanip>
#include <sstream>
#include "utils/utils.h"
#include "ws/ws_hub.h"
#include "core/http_response.h"
#include "core/worker_pool.h"
namespace taskhub {
    void SystemHandler::setup_routes(httplib::Server &server)
    {
        server.Get("/api/health", SystemHandler::health);
        server.Get("/api/info",   SystemHandler::info);
        server.Get("/api/stats",  SystemHandler::stats);
        server.Get("/api/debug/broadcast", SystemHandler::broadcast);
    }

    // Request: GET /api/health
    // Response: 200 {"code":0,"message":"ok","data":{"status":"healthy","time":"<ISO>","uptime_sec":<int>}}
    void SystemHandler::health(const httplib::Request &, httplib::Response &res) {
        Logger::info("GET /api/health");

        nlohmann::json data;
        data["status"] = "healthy";

        data["time"] = utils::now_string();
        data["uptime_sec"] = 1; // TODO: Replace with actual uptime calculation

        resp::ok(res, data);
    }

    // Request: GET /api/info
    // Response: 200 {"code":0,"message":"ok","data":{"name":"TaskHub Server","version":"0.1.0","build_time": "...","commit":"dev-local"}}
    void SystemHandler::info(const httplib::Request &, httplib::Response &res) {
        Logger::info("GET /api/info");

        nlohmann::json data;
        data["name"]       = "TaskHub Server";
        data["version"]    = "0.1.0";
        data["build_time"] = __DATE__ " " __TIME__;
        data["commit"]     = "dev-local";

        resp::ok(res, data);
    }
    // Request: GET /api/debug/broadcast
    // Response: 200 {"code":0,"message":"ok","data":null}; 副作用：向所有 WS session 推送 {"event":"debug","data":{"msg":"hello from TaskHub WS"}}
    void SystemHandler::broadcast(const httplib::Request &req, httplib::Response &res)
    {
        using nlohmann::json;
        json j;
        j["event"] = "debug";
        j["data"]  = { {"msg", "hello from TaskHub WS"} };
        Logger::info("HTTP /api/debug/broadcast called");
        Logger::info("Broadcasting: " + j.dump());
        WsHub::instance().broadcast(j.dump());
    
        resp::ok(res);  // 走你已封装的统一响应
    }
    // Request: GET /api/stats
    // Response: 200 {"code":0,"message":"ok","data":{"worker_pool":{"workers":int,"queued":int,"running":int,"submitted":int,"finished":int},"ws":{"sessions":int}}}
    void SystemHandler::stats(const httplib::Request &, httplib::Response &res)
    {
        auto poolStats = WorkerPool::instance()->stats();
        nlohmann::json data;
        data["worker_pool"]["workers"]   = poolStats.workers;
        data["worker_pool"]["queued"]    = poolStats.queued;
        data["worker_pool"]["running"]   = poolStats.running;
        data["worker_pool"]["submitted"] = poolStats.submitted;
        data["worker_pool"]["finished"]  = poolStats.finished;

        data["ws"]["sessions"] = WsHub::instance().session_count();

        resp::ok(res, data);
    }
}
