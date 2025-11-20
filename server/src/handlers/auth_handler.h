#pragma once

#include "httplib.h"

namespace taskhub {

class AuthHandler {
public:
    static void login(const httplib::Request& req,
                      httplib::Response& res);
};

} // namespace taskhub