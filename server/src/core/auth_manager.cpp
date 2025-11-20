#include "auth_manager.h"
#include "core/logger.h"
#include <random>
namespace taskhub {
    AuthManager &AuthManager::instance()
    {
        // TODO: 在此处插入 return 语句
        static AuthManager inst;
        return inst;
    }

    void AuthManager::init()
    {
         // ★ M4：先写死一个 admin 用户
    // 后面可以改成从 Config 加载
        std::lock_guard<std::mutex> lock(m_mutex);

        User admin;
        admin.username = "admin";
        admin.password = "123456";   // TODO: use hash in future
        admin.is_admin = true;

        m_users[admin.username] = admin;

        Logger::info("AuthManager initialized with default admin user");
    }

    std::optional<std::string> taskhub::AuthManager::login(const std::string &username, const std::string &password)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_users.find(username);
        if (it == m_users.end()) {
            return std::nullopt;
        }
        const User& user = it->second;

        if (user.password != password) {
            return std::nullopt;
        }

        // 生成 token
        std::string token = generate_token();

        // 填写 session
        Session s;
        s.username = user.username;
        auto now = std::chrono::system_clock::now();
        s.expire_time = now + std::chrono::seconds(m_token_ttl_sec);

        m_sessions[token] = s;

        Logger::info("User " + username + " login success, token created");

        return token;
    }
    std::optional<User> AuthManager::validate_token(const std::string &token)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_sessions.find(token);
        if (it == m_sessions.end()) {
            return std::nullopt;
        }
    
        auto now = std::chrono::system_clock::now();
        if (now > it->second.expire_time) {
            // 过期了，删掉
            Logger::info("Token expired for user: " + it->second.username);
            m_sessions.erase(it);
            return std::nullopt;
        }
    
        // 找用户
        auto uit = m_users.find(it->second.username);
        if (uit == m_users.end()) {
            return std::nullopt;
        }
    
        return uit->second;
    }
    std::string AuthManager::generate_token()
    {
           // 简单随机字符串：16 字节的 [a-zA-Z0-9]
        static const char charset[] =
        "0123456789"
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

        thread_local std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<std::size_t> dist(0, sizeof(charset) - 2);

        std::string token;
        token.reserve(32);
        for (int i = 0; i < 32; ++i) {
            token.push_back(charset[dist(rng)]);
        }
        return token;
    }
}