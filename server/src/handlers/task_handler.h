//
// Created by 曹宸源 on 2025/11/13.
//

#ifndef TASKHUB_TASK_HANDLER_H
#define TASKHUB_TASK_HANDLER_H
#include "httplib.h"
namespace taskhub {
    class TaskHandler {
    public:
        static void create(const httplib::Request& req, httplib::Response& res);
        static void list(const httplib::Request& req, httplib::Response& res);
        static void detail(const httplib::Request& req, httplib::Response& res);
        static void cancel_task(const httplib::Request& req, httplib::Response& res);
    };

}


#endif //TASKHUB_TASK_HANDLER_H
