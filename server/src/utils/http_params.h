#pragma once

#include <string>
#include <httplib.h>

namespace taskhub::http_params {

inline long long get_ll(const httplib::Request& req, const std::string& key, long long def = 0) {
    if (!req.has_param(key)) return def;
    try {
        return std::stoll(req.get_param_value(key));
    } catch (...) {
        return def;
    }
}

inline int get_int(const httplib::Request& req, const std::string& key, int def = 0) {
    if (!req.has_param(key)) return def;
    try {
        return std::stoi(req.get_param_value(key));
    } catch (...) {
        return def;
    }
}

} // namespace taskhub::http_params
