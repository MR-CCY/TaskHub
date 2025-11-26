#pragma once

#include <string>
#include <unordered_map>
#include <optional>
#include <mutex>
#include "user.h"

namespace taskhub {

class AuthManager {
public:
    static AuthManager& instance();

    // 初始化用户列表（可以从 config 来，M4 先写死）
    void init();

    // 登录：验证用户名密码，成功则返回 token
    std::optional<std::string> login(const std::string& username,
                                     const std::string& password);

    // 校验 token：合法且未过期 -> 返回 User
    std::optional<User> validate_token(const std::string& token);

    // 从 httplib::Request 中解析 Authorization 头并校验
    template <typename Request>
    std::optional<User> user_from_request(const Request& req);

private:
    AuthManager() = default;
    AuthManager(const AuthManager&) = delete;
    AuthManager& operator=(const AuthManager&) = delete;

    std::string generate_token();      // 内部生成随机 token

private:
    std::mutex m_mutex;
    // 内存用户表（按用户名）
    std::unordered_map<std::string, User> m_users;

    // token -> session
    std::unordered_map<std::string, Session> m_sessions;

    // token 有效期（秒）
    // int m_token_ttl_sec{2 * 60 * 60}; // 默认两小时
    int m_token_ttl_sec{30}; // 默认30秒
};

template <typename Request>
inline std::optional<User> AuthManager::user_from_request(const Request &req)
{
    auto auth = req.get_header_value("Authorization");
    if (auth.rfind("Bearer ", 0) != 0) {
        return std::nullopt;
    }
    std::string token = auth.substr(7);
    return validate_token(token);
}

} // namespace taskhub