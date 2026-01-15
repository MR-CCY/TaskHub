//
// Created by 曹宸源 on 2025/11/11.
//
#include "router.h"
#include "auth/auth_manager.h"
#include "core/http_response.h"
#include "handlers/handlers.h"
#include "httplib.h"
namespace taskhub {
void Router::setup_routes(httplib::Server &server) {
  // 全局拦截：除登录和 Worker 相关接口外，其余路径统一校验 token
  server.set_pre_routing_handler(
      [](const httplib::Request &req, httplib::Response &res) {
        const auto &path = req.path;
        const bool skipAuth =
            path == "/api/login" ||
            path.rfind("/api/workers", 0) == 0 || // register/heartbeat/list
            path.rfind("/api/worker/execute", 0) == 0;

        if (skipAuth) {
          return httplib::Server::HandlerResponse::Unhandled;
        }

        auto user_opt = AuthManager::instance().user_from_request(req);
        if (!user_opt) {
          resp::unauthorized(res);
          return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
      });
  // 系统信息类接口
  SystemHandler::setup_routes(server);
  // 认证相关接口
  AuthHandler::setup_routes(server);
  // 任务类接口
  TaskHandler::setup_routes(server);
  // 定时类相关接口
  CronHandler::setup_routes(server);
  // Rmote Worker 相关接口
  WorkHandler::setup_routes(server);
  // 日志相关接口
  LogHandler::setup_routes(server);
  // 模板相关接口
  TemplateHandler::setup_routes(server);
  // 任务模板相关接口
  TaskTemplateHandler::setup_routes(server);
}
} // namespace taskhub
