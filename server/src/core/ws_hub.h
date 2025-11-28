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

private:
    WsHub() = default;
    WsHub(const WsHub&) = delete;
    WsHub& operator=(const WsHub&) = delete;

    std::mutex mtx_;
    std::vector<std::weak_ptr<WsSession>> sessions_;
};

} // namespace taskhub