#include "ws_hub.h"
#include "core/logger.h"
#include <boost/beast/websocket.hpp>
#include "core/ws_server_beast.h"

namespace taskhub {
    WsHub &taskhub::WsHub::instance()
    {
        // TODO: 在此处插入 return 语句
        static WsHub instance;
        return instance;
    }

    void taskhub::WsHub::add_session(std::shared_ptr<WsSession> session)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        sessions_.emplace_back(session);
        Logger::info("WsHub: session added, count=" + std::to_string(sessions_.size()));
    }

    void taskhub::WsHub::remove_session(const std::shared_ptr<WsSession> &session)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        // 移除指定 session
        sessions_.erase(std::remove_if(sessions_.begin(), sessions_.end(),
                                       [&session](const std::weak_ptr<WsSession>& ws_weak) {
                                           auto ws_shared = ws_weak.lock();
                                           return !ws_shared || ws_shared == session;
                                       }),
                        sessions_.end());
        Logger::info("WsHub: session removed, count=" + std::to_string(sessions_.size()));
    }

    void taskhub::WsHub::broadcast(const std::string &text)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        Logger::info("WsHub: broadcast to " + std::to_string(sessions_.size()) + " session(s)");
        for (auto it = sessions_.begin(); it != sessions_.end();) {
            if (auto s = it->lock()) {
                s->send(text);  // ★ 由 WsSession 提供 send
                ++it;
            } else {
                it = sessions_.erase(it);
            }
        }
    }
    void WsHub::broadcast(const std::string &channel, const std::string &text)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        for (auto& weak : sessions_) {
            if (auto s = weak.lock()) {
                if (s->subscribed(channel)) {
                    s->send_text(text);
                }
            }
        }
    }
}