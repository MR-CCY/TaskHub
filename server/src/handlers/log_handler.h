#pragma once

#include <memory>
#include "httplib.h"

namespace taskhub {

class LogHandler {

public:
/// 注册 Worker 相关的 HTTP 路由
    static void setup_routes(httplib::Server& server);
private:
    static void logs(const httplib::Request& req, httplib::Response& res);
};

} // namespace taskhub::handlers