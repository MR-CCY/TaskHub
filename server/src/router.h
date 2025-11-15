//
// Created by 曹宸源 on 2025/11/11.
//

#ifndef TASKHUB_ROUTER_H
#define TASKHUB_ROUTER_H
#include "server_app.h"
//注册路由

namespace taskhub {
    class Router {
    public:
        // 只提供静态方法，不需要实例化
        Router() = delete;
        ~Router() = delete;
        static void setup_routes(httplib::Server& server);
    private:

    };
}


#endif //TASKHUB_ROUTER_H
