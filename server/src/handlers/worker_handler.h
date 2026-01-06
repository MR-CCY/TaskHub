#pragma once

#include <memory>
#include "httplib.h"

namespace taskhub {

class WorkHandler {

public:
/// 注册 Worker 相关的 HTTP 路由
    static void setup_routes(httplib::Server& server);
private:
    static void workers_register(const httplib::Request& req, httplib::Response& res);
    static void workers_heartbeat(const httplib::Request& req, httplib::Response& res);
    static void workers_list(const httplib::Request& req, httplib::Response& res);
    static void workers_connected(const httplib::Request& req, httplib::Response& res);
    static void workers_proxy_logs(const httplib::Request& req, httplib::Response& res);
    static void workers_proxy_task_runs(const httplib::Request& req, httplib::Response& res);
    static void workers_proxy_task_events(const httplib::Request& req, httplib::Response& res);
    static void worker_execute(const httplib::Request& req, httplib::Response& res);
};

} // namespace taskhub::handlers
