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
inline bool is_retryable_http_status(int status) {
    return status >= 500 && status <= 599;
}

inline std::string http_error_to_string(httplib::Error err) {
    switch (err) {
        case httplib::Error::Connection:
            return "connection";
        case httplib::Error::Read:
            return "read";
        case httplib::Error::Write:
            return "write";
        case httplib::Error::ConnectionTimeout:
            return "timeout";
        case httplib::Error::Canceled:
            return "canceled";
        case httplib::Error::SSLConnection:
            return "ssl_connection";
        case httplib::Error::SSLServerVerification:
            return "ssl_verify";
        case httplib::Error::SSLServerHostnameVerification:
            return "ssl_hostname_verify";
        default:
            return "error_" + std::to_string(static_cast<int>(err));
    }
}
} // namespace taskhub::http_params
