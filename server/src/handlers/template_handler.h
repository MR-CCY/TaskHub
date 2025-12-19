#pragma once

#include <memory>
#include "httplib.h"

namespace taskhub {

class TemplateHandler {

public:
/// 注册 Worker 相关的 HTTP 路由
    static void setup_routes(httplib::Server& server);
private:
    static void create(const httplib::Request& req, httplib::Response& res);
    static void get(const httplib::Request& req, httplib::Response& res);
    static void list(const httplib::Request& req, httplib::Response& res);
    static void delete_(const httplib::Request& req, httplib::Response& res);
    static void update(const httplib::Request& req, httplib::Response& res);
    static void render(const httplib::Request& req, httplib::Response& res);
    static void run(const httplib::Request& req, httplib::Response& res);
    static void runAsync(const httplib::Request& req, httplib::Response& res);
};

} // namespace taskhub::handlers
