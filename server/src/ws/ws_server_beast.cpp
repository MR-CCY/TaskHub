#include "ws_server_beast.h"
#include "log/logger.h"
#include "ws_hub.h"
#include "ws_protocol.h"
#include <boost/beast/core/buffers_to_string.hpp>
#include <json.hpp>
#include "auth/auth_manager.h"
namespace taskhub {
    WsSession::WsSession(tcp::socket socket)
    : ws_(std::move(socket)),
      exec_(ws_.get_executor()) {

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
                net::dispatch(self->exec_, [self]() {
                    self->do_read();
                });
            });
    }

    void WsSession::send(const std::string &text)
    {
        auto self = shared_from_this();
        net::post(exec_, [self, text]() {
            const bool writing = !self->write_queue_.empty();
            if (self->write_queue_.size() >= WsSession::kMaxPendingMessages) {
                Logger::warn("WsSession backpressure: closing slow client");
                beast::error_code ec;
                self->ws_.close(websocket::close_reason("too many pending messages"), ec);
                WsHub::instance().remove_session(self);
                return;
            }
            self->write_queue_.push_back(text);
            if (!writing) {
                self->do_write();
            }
        });
    }
    void WsSession::send_text(const std::string &text)
    {
        send(text);
    }
    bool WsSession::subscribed(const std::string &channel) const
    {
        std::lock_guard<std::mutex> lock(sub_mtx_);
        return subscriptions_.count(channel) > 0;
    }
    void WsSession::subscribe(const std::string &channel)
    {
        std::lock_guard<std::mutex> lock(sub_mtx_);
        subscriptions_.insert(channel);
    }
    void WsSession::unsubscribe(const std::string &channel)
    {
        std::lock_guard<std::mutex> lock(sub_mtx_);
        subscriptions_.erase(channel);
    }


    void WsSession::do_read()
    {
        ws_.async_read(
            buffer_,
            [self = shared_from_this()](beast::error_code ec, std::size_t bytes) {
                self->on_read(ec, bytes);
            });
    }
    void WsSession::on_read(beast::error_code ec, std::size_t bytes_transferred)
    {
  
        if (ec == websocket::error::closed) {
            Logger::error("WsSession read error: " + ec.message() + " (" + std::to_string(ec.value()) + ")");
            // 连接关闭
            WsHub::instance().remove_session(shared_from_this());
            return;
        }
        if (ec) {
            Logger::error("WsSession read error: " + ec.message());
            WsHub::instance().remove_session(shared_from_this());
            return;
        }

        // 解析客户端指令（订阅/退订等）
        const std::string payload = beast::buffers_to_string(buffer_.data());
        buffer_.consume(buffer_.size());
        handle_command(payload);
        do_read();  // 继续读下一条
    }
    void WsSession::do_write()
    {
        auto self = shared_from_this();
        ws_.text(true);
        ws_.async_write(
            net::buffer(write_queue_.front()),
            [self](beast::error_code ec, std::size_t /*bytes_written*/) {
                if (ec) {
                    Logger::error("WsSession send error: " + ec.message());
                    WsHub::instance().remove_session(self);
                    return;
                }
                self->write_queue_.pop_front();
                if (!self->write_queue_.empty()) {
                    self->do_write();
                }
            });
    }
    // 处理客户端指令：
    //   - 首条消息必须包含 "token":"<jwt>"，校验通过后 authed_=true（可只发 token，收到 {"type":"authed"}）。
    //   - 订阅/退订：{"token":"...","op":"subscribe|unsubscribe","topic":"task_logs|task_events","task_id":"t1","run_id":"r1?"}
    //   - 心跳：{"token":"...","op":"ping"} -> 返回 {"type":"pong"}
    // 收到的消息解析失败或超过速率会直接关闭连接。
    void WsSession::handle_command(const std::string &payload)
    {
        constexpr std::size_t kMaxCommandsPerWindow = 30;
        constexpr auto kWindow = std::chrono::seconds(10);
        const auto now = std::chrono::steady_clock::now();
        // rate limit client commands to protect server resources
        while (!cmd_timestamps_.empty() && now - cmd_timestamps_.front() > kWindow) {
            cmd_timestamps_.pop_front();
        }
        if (cmd_timestamps_.size() >= kMaxCommandsPerWindow) {
            Logger::warn("WsSession rate limit exceeded, closing connection");
            beast::error_code ec;
            ws_.close(websocket::close_reason("rate limit"), ec);
            WsHub::instance().remove_session(shared_from_this());
            return;
        }
        cmd_timestamps_.push_back(now);
        try {
            auto j = nlohmann::json::parse(payload);
            // 简单鉴权：要求携带 token 并校验一次
            if (!authed_) {
                auto it = j.find("token");
                if (it == j.end() || !it->is_string()) {
                    Logger::warn("WsSession missing token, closing");
                    beast::error_code ec;
                    ws_.close(websocket::close_reason("unauthorized"), ec);
                    WsHub::instance().remove_session(shared_from_this());
                    return;
                }
                auto user = AuthManager::instance().validate_token(it->get<std::string>());
                if (!user) {
                    Logger::warn("WsSession invalid token, closing");
                    beast::error_code ec;
                    ws_.close(websocket::close_reason("unauthorized"), ec);
                    WsHub::instance().remove_session(shared_from_this());
                    return;
                }
                authed_ = true;
                // 纯认证消息：允许没有 op 的第一条消息，避免误报错误
                if (!j.contains("op")) {
                    send_text(R"({"type":"authed"})");
                    return;
                }
            }
            auto cmd = ws::parseClientCommand(j);
            switch (cmd.op) {
                case ws::WsOp::Subscribe: {
                    std::string ch;
                    if (cmd.topic == ws::WsTopic::TaskLogs) {
                        ch = ws::channelTaskLogs(cmd.taskId, cmd.runId);
                    } else if (cmd.topic == ws::WsTopic::TaskEvents) {
                        ch = ws::channelTaskEvents(cmd.taskId, cmd.runId);
                    }
                    if (!ch.empty()) {
                        subscribe(ch);
                    }
                    break;
                }
                case ws::WsOp::Unsubscribe: {
                    std::string ch;
                    if (cmd.topic == ws::WsTopic::TaskLogs) {
                        ch = ws::channelTaskLogs(cmd.taskId, cmd.runId);
                    } else if (cmd.topic == ws::WsTopic::TaskEvents) {
                        ch = ws::channelTaskEvents(cmd.taskId, cmd.runId);
                    }
                    if (!ch.empty()) {
                        unsubscribe(ch);
                    }
                    break;
                }
                case ws::WsOp::Ping:
                    send_text(R"({"type":"pong"})");
                    break;
                default:
                    break;
            }
        } catch (const std::exception& e) {
            Logger::error(std::string("WsSession handle_command parse error: ") + e.what());
        } catch (...) {
            Logger::error("WsSession handle_command parse error: unknown");
        }
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
        running_ = true;
    }

    WsServer::~WsServer()
    {
        stop();
    }
    bool WsServer::start()
    {
       
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
