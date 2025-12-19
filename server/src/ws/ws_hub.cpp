#include "ws_hub.h"
#include "log/logger.h"
#include <boost/beast/websocket.hpp>
#include "ws/ws_server_beast.h"

namespace taskhub {
    WsHub &taskhub::WsHub::instance()
    {
        // TODO: 在此处插入 return 语句
        static WsHub instance;
        return instance;
    }

    void taskhub::WsHub::add_session(std::shared_ptr<WsSession> session)
    {
        std::size_t cnt = 0;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            sessions_.emplace_back(session);
            cnt = sessions_.size();
        }
        Logger::info("WsHub: session added, count=" + std::to_string(cnt));
    }

    void taskhub::WsHub::remove_session(const std::shared_ptr<WsSession> &session)
    {
        std::size_t cnt = 0;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            // 移除指定 session
            sessions_.erase(std::remove_if(sessions_.begin(), sessions_.end(),
                                           [&session](const std::weak_ptr<WsSession>& ws_weak) {
                                               auto ws_shared = ws_weak.lock();
                                               return !ws_shared || ws_shared == session;
                                           }),
                            sessions_.end());
            cnt = sessions_.size();
        }
        Logger::info("WsHub: session removed, count=" + std::to_string(cnt));
    }

    void taskhub::WsHub::broadcast(const std::string &text)
    {
        std::lock_guard<std::mutex> lock(mtx_);
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
    std::size_t WsHub::session_count() const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        std::size_t alive = 0;
        for (const auto& weak : sessions_) {
            if (weak.lock()) {
                ++alive;
            }
        }
        return alive;
    }
}
