#pragma once
#include "httplib.h"

namespace taskhub {

class CronHandler {
public:
    // 在 Router::setup_routes 里调用
    static void setup_routes(httplib::Server& server);

private:
    // GET /api/cron/jobs
    static void list_jobs(const httplib::Request& req,
                          httplib::Response& res);

    // POST /api/cron/jobs
    static void create_job(const httplib::Request& req,
                           httplib::Response& res);

    // DELETE /api/cron/jobs/:id
    static void delete_job(const httplib::Request& req,
                           httplib::Response& res);
};

} // namespace taskhub::handlers