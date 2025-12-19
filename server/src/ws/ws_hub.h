#pragma once
#include <mutex>
#include <vector>
#include <memory>
#include <string>

namespace taskhub {

class WsSession;

class WsHub {
public:
    static WsHub& instance();

    void add_session(std::shared_ptr<WsSession> session);
    void remove_session(const std::shared_ptr<WsSession>& session);

    // 对所有在线 session 发送一条文本消息
    void broadcast(const std::string& text);
    // ✅ 新接口：按 channel 广播（M12.2 核心）
    void broadcast(const std::string& channel, const std::string& text);
    std::size_t session_count() const;
private:
    WsHub() = default;
    WsHub(const WsHub&) = delete;
    WsHub& operator=(const WsHub&) = delete;

    mutable std::mutex mtx_;
    std::vector<std::weak_ptr<WsSession>> sessions_;
};

} // namespace taskhub
