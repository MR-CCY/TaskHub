// core/user.h
#pragma once

#include <string>
#include <chrono>

namespace taskhub {

struct User {
    std::string username;
    std::string password;   // 现在先明文，后面可以改 hash
    bool        is_admin{false};
};

struct Session {
    std::string username;
    std::chrono::system_clock::time_point expire_time;
};

} // namespace taskhub