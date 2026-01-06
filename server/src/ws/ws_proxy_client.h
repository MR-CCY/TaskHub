#pragma once

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <deque>
#include <memory>
#include <string>
#include <thread>
#include <atomic>

namespace taskhub {

class WsSession;

class WsProxyClient : public std::enable_shared_from_this<WsProxyClient> {
public:
    static constexpr std::size_t kMaxPendingMessages = 512;

    WsProxyClient(std::string host,
                  unsigned short port,
                  std::string token,
                  std::weak_ptr<WsSession> owner);

    bool start(std::string* error = nullptr);
    void stop();
    void send(const std::string& text);
    bool connected() const { return connected_.load(std::memory_order_acquire); }

private:
    void do_read();
    void on_read(boost::beast::error_code ec, std::size_t bytes_transferred);
    void do_write();

    std::string host_;
    unsigned short port_{0};
    std::string token_;
    std::weak_ptr<WsSession> owner_;

    boost::asio::io_context ioc_;
    boost::beast::websocket::stream<boost::asio::ip::tcp::socket> ws_;
    boost::beast::flat_buffer buffer_;
    std::deque<std::string> write_queue_;
    std::thread thread_;
    std::atomic_bool connected_{false};
    std::atomic_bool stopping_{false};
};

} // namespace taskhub
