//
// Created by 曹宸源 on 2025/11/11.
//

#include "router.h"
#include "httplib.h"
#include "handlers/handlers.h"
namespace taskhub {
    void Router::setup_routes(httplib::Server& server){
        // 系统信息类接口
        server.Get("/api/health", SystemHandler::health);
        server.Get("/api/info",   SystemHandler::info);
        server.Post("/api/tasks", TaskHandler::create);
        server.Get("/api/tasks",  TaskHandler::list);
        // 正则匹配数字 ID
        server.Get(R"(/api/tasks/(\d+))", TaskHandler::detail);
    }
}