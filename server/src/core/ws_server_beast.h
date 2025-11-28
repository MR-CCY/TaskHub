#pragma once
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <memory>
#include <thread>
#include <atomic>
#include <string>

namespace taskhub {

namespace net  = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;

class WsSession : public std::enable_shared_from_this<WsSession> {
public:
    explicit WsSession(tcp::socket socket);

    void start();
    void send(const std::string& text);   // 给当前 session 发消息

private:
    void do_read();
    void on_read(beast::error_code ec, std::size_t bytes_transferred);

    websocket::stream<tcp::socket> ws_;
    beast::flat_buffer buffer_;
    std::mutex send_mtx_;
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