#pragma once

#include "httplib.h"
#include "json.hpp"
#include <string>

namespace taskhub::resp {

using Json = nlohmann::json;

// 最底层：统一构造 {"code", "message", "data"} 并写入 Response
inline void json(httplib::Response& res,
                 int code,
                 const Json& data,
                 const std::string& message,
                 int http_status = 200)
{
    Json body;
    body["code"]    = code;
    body["message"] = message;
    if (data.is_null()) {
        body["data"] = nullptr;
    } else {
        body["data"] = data;
    }

    res.status = http_status;
    res.set_header("Content-Type", "application/json; charset=utf-8");
    res.set_content(body.dump(), "application/json; charset=utf-8");
}

// 成功：无 data
inline void ok(httplib::Response& res, const std::string& message = "ok") {
    json(res, 0, Json(), message, 200);
}

// 成功：有 data
inline void ok(httplib::Response& res, const Json& data, const std::string& message = "ok") {
    json(res, 0, data, message, 200);
}

// 通用错误（业务 code + http 状态码可选）
inline void error(httplib::Response& res,
                  int code,
                  const std::string& message,
                  int http_status = 200)
{
    json(res, code, Json(), message, http_status);
}

// Bad Request（参数错误等）
inline void bad_request(httplib::Response& res,
                        const std::string& message = "bad request",
                        int code = 400)
{
    json(res, code, Json(), message, 400);
}

// 未授权
inline void unauthorized(httplib::Response& res,
                         const std::string& message = "unauthorized",
                         int code = 401)
{
    json(res, code, Json(), message, 401);
}

// 未找到
inline void not_found(httplib::Response& res,
                      const std::string& message = "not found",
                      int code = 404)
{
    json(res, code, Json(), message, 404);
}

} // namespace taskhub::resp