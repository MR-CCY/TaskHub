#pragma once
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <memory>
#include <thread>
#include <atomic>
#include <string>
#include <unordered_set>
#include <mutex>
#include <deque>

namespace taskhub {

namespace net  = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;

class WsSession : public std::enable_shared_from_this<WsSession> {
public:
    static constexpr std::size_t kMaxPendingMessages = 512;

    explicit WsSession(tcp::socket socket);

    void start();
    void send(const std::string& text);        // 给当前 session 发消息（异步、串行）
    void send_text(const std::string& text);   // alias for send
    bool subscribed(const std::string& channel) const;

private:
    void handle_command(const std::string& payload);
    void subscribe(const std::string& channel);
    void unsubscribe(const std::string& channel);

    void do_read();
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    void do_write();

    websocket::stream<tcp::socket> ws_;
    net::any_io_executor exec_;
    beast::flat_buffer buffer_;
    std::deque<std::string> write_queue_;
    mutable std::mutex sub_mtx_;
    std::unordered_set<std::string> subscriptions_;
};

class WsServer {
public:
    WsServer(const std::string& host, unsigned short port);
    ~WsServer();

    bool start();  // 在独立线程中运行 io_context
    void stop();

private:
    void do_accept();

    net::io_context ioc_;
    tcp::acceptor acceptor_;
    std::thread thread_;
    std::atomic<bool> running_{false};
};

} // namespace taskhub
