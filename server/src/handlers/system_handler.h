//
// Created by 曹宸源 on 2025/11/13.
//

#ifndef TASKHUB_SYSTEM_HANDLER_H
#define TASKHUB_SYSTEM_HANDLER_H
#pragma once

#include "httplib.h"

namespace taskhub {
    class SystemHandler {
    public:
        static void setup_routes(httplib::Server& server);
        static void health(const httplib::Request& req,httplib::Response& res);

        static void info(const httplib::Request& req,httplib::Response& res);
        static void broadcast(const httplib::Request& req,httplib::Response& res);
    };
}
#endif //TASKHUB_SYSTEM_HANDLER_H
