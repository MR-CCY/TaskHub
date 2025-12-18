//
// Created by 曹宸源 on 2025/11/11.
//
#include "router.h"
#include "httplib.h"
#include "handlers/handlers.h"
namespace taskhub {
    void Router::setup_routes(httplib::Server& server){
        // 系统信息类接口
        SystemHandler::setup_routes(server);
        // 认证相关接口
        AuthHandler::setup_routes(server);
        // 任务类接口
        TaskHandler::setup_routes(server);
        //定时类相关接口
        CronHandler::setup_routes(server);
        //Rmote Worker 相关接口
        WorkHandler::setup_routes(server);
        // 日志相关接口
        LogHandler::setup_routes(server);
        // 模板相关接口
        TemplateHandler::setup_routes(server);
    }
}