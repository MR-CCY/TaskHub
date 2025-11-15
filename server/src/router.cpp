//
// Created by 曹宸源 on 2025/11/11.
//

#include "router.h"
#include "../../third_party/httplib/httplib.h"
#include "handlers/system_handler.h"
namespace taskhub {
    void Router::setup_routes(httplib::Server& server){
        // 系统信息类接口
        server.Get("/api/health", SystemHandler::health);
        server.Get("/api/info",   SystemHandler::info);
    }
}