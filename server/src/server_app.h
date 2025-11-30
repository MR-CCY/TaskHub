//
// Created by 曹宸源 on 2025/11/11.
//

#pragma once
#include <memory>
#include "httplib.h"
#include "core/ws_server_beast.h"
#include "core/worker_pool.h"
namespace taskhub {

class ServerApp {
public:
    ServerApp();
    ~ServerApp();

    // 程序主入口
    int run();

private:
    void init_config();
    void init_logger();
    void init_http_server();
    void setup_routes();
    void init_db(); 
private:
    // HTTP 服务对象
    std::unique_ptr<httplib::Server> m_server;
    std::unique_ptr<taskhub::WsServer> m_wsServer;
    // 服务运行参数
    std::string m_host;
    int m_port;
};

} // namespace taskhub
