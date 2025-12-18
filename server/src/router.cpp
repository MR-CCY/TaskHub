//
// Created by 曹宸源 on 2025/11/11.
//

#include "router.h"
#include "httplib.h"
#include "handlers/handlers.h"
#include "ws/ws_hub.h"
#include  "core/http_response.h"
#include   "log/logger.h"
namespace taskhub {
    void Router::setup_routes(httplib::Server& server){
        // 系统信息类接口
        server.Get("/api/health", SystemHandler::health);
        server.Get("/api/info",   SystemHandler::info);
        server.Post("/api/tasks", TaskHandler::create);
        server.Get("/api/tasks",  TaskHandler::list);
        server.Post("/api/login", AuthHandler::login);
        // 正则匹配数字 ID
        server.Get(R"(/api/tasks/(\d+))", TaskHandler::detail);
        server.Get("/api/debug/broadcast", [](const httplib::Request& req, httplib::Response& res) {
            using nlohmann::json;
            json j;
            j["event"] = "debug";
            j["data"]  = { {"msg", "hello from TaskHub WS"} };
            Logger::info("HTTP /api/debug/broadcast called");
            Logger::info("Broadcasting: " + j.dump());
            WsHub::instance().broadcast(j.dump());
        
            resp::ok(res);  // 走你已封装的统一响应
        });
        //M7
        server.Post(R"(/api/tasks/(\d+)/cancel)", TaskHandler::cancel_task);
        //M8
        server.Post("/api/dag/run", TaskHandler::runDag);
        //M10
        CronHandler::setup_routes(server);
        //M11
        WorkHandler::setup_routes(server);
        LogHandler::setup_routes(server);
        TemplateHandler::setup_routes(server);
    }
}