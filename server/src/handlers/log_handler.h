#pragma once

#include <memory>
#include "httplib.h"

namespace taskhub {

class LogHandler {

public:
    static void setup_routes(httplib::Server& server);
    static void query(const httplib::Request& req, httplib::Response& res);
private:
};

} // namespace taskhub
