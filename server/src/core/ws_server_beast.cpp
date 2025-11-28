#include "ws_server_beast.h"
#include "logger.h"
#include "ws_hub.h"
namespace taskhub {
    WsSession::WsSession(tcp::socket socket)
    : ws_(std::move(socket)) {

    }

    void WsSession::start()
    {
        // 接受 WebSocket 握手（异步）
        ws_.async_accept(
            [self = shared_from_this()](beast::error_code ec) {
                if (ec) {
                    Logger::error("WsSession accept error: " + ec.message());
                    return;
                }
                Logger::info("WsSession accepted new client");      // ★ 日志1
                // 握手成功，加入 Hub
                WsHub::instance().add_session(self);
                self->do_read();
            });
    }

    void WsSession::send(const std::string &text)
    {
        std::lock_guard<std::mutex> lock(send_mtx_);
        beast::error_code ec;
        ws_.text(true);
        ws_.write(net::buffer(text), ec);
        if (ec) {
            Logger::error("WsSession send error: " + ec.message());
        }
    }


    void WsSession::do_read()
    {
        ws_.async_read(buffer_,
            [self = shared_from_this()](beast::error_code ec, std::size_t bytes) {
                self->on_read(ec, bytes);
            });
    }
    void WsSession::on_read(beast::error_code ec, std::size_t bytes_transferred)
    {
        if (ec == websocket::error::closed) {
            // 连接关闭
            WsHub::instance().remove_session(shared_from_this());
            return;
        }
        if (ec) {
            Logger::error("WsSession read error: " + ec.message());
            WsHub::instance().remove_session(shared_from_this());
            return;
        }
    
        // 当前我们不需要处理客户端发来的内容，可以忽略
        buffer_.consume(buffer_.size());
    
        // 继续读下一条
        do_read();
    }

    WsServer::WsServer(const std::string &host, unsigned short port)
    : ioc_(1),acceptor_(ioc_)
    {
        tcp::endpoint endpoint(net::ip::make_address(host), port);
        beast::error_code ec;

        acceptor_.open(endpoint.protocol(), ec);
        if (ec) {
            Logger::error("WsServer open error: " + ec.message());
            return;
        }

        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec) {
            Logger::error("WsServer set_option error: " + ec.message());
            return;
        }

        acceptor_.bind(endpoint, ec);
        if (ec) {
            Logger::error("WsServer bind error: " + ec.message());
            return;
        }

        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec) {
            Logger::error("WsServer listen error: " + ec.message());
            return;
        }
    }

    WsServer::~WsServer()
    {
        stop();
    }
    bool WsServer::start()
    {
        running_ = true;
        do_accept();
        thread_ = std::thread([this]() {
            Logger::info("WsServer io_context run");
            ioc_.run();
            Logger::info("WsServer io_context exit");
        });
        return true;
    }
    void WsServer::stop()
    {
        if (!running_) return;
        running_ = false;
        beast::error_code ec;
        acceptor_.close(ec);
        ioc_.stop();
        if (thread_.joinable()) thread_.join();
    }
    void WsServer::do_accept()
    {
        acceptor_.async_accept(
            [this](beast::error_code ec, tcp::socket socket) {
                if (!acceptor_.is_open()) {
                    return;
                }
    
                if (!ec) {
                    auto session = std::make_shared<WsSession>(std::move(socket));
                    session->start();
                } else {
                    Logger::error("WsServer accept error: " + ec.message());
                }
    
                if (running_) {
                    do_accept();  // 继续接受下一连接
                }
            });
    }


} // namespace taskhub
