#include "ws_proxy_client.h"
#include "ws_server_beast.h"
#include "log/logger.h"
#include "json.hpp"

namespace taskhub {

namespace net = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;
using json = nlohmann::json;

WsProxyClient::WsProxyClient(std::string host,
                             unsigned short port,
                             std::string token,
                             std::weak_ptr<WsSession> owner)
    : host_(std::move(host)),
      port_(port),
      token_(std::move(token)),
      owner_(std::move(owner)),
      ws_(ioc_)
{
}

bool WsProxyClient::start(std::string* error)
{
    try {
        tcp::resolver resolver(ioc_);
        auto results = resolver.resolve(host_, std::to_string(port_));
        net::connect(ws_.next_layer(), results);
        ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
        ws_.handshake(host_, "/");
        connected_.store(true, std::memory_order_release);

        if (!token_.empty()) {
            json j;
            j["token"] = token_;
            ws_.write(net::buffer(j.dump()));
        }
    } catch (const std::exception& e) {
        if (error) {
            *error = e.what();
        }
        return false;
    }

    do_read();
    thread_ = std::thread([this]() {
        ioc_.run();
    });
    return true;
}

void WsProxyClient::stop()
{
    if (stopping_.exchange(true)) {
        return;
    }
    connected_.store(false, std::memory_order_release);

    auto self = shared_from_this();
    net::post(ioc_, [self]() {
        beast::error_code ec;
        self->ws_.close(websocket::close_reason("proxy_stop"), ec);
    });

    ioc_.stop();
    if (thread_.joinable() && thread_.get_id() != std::this_thread::get_id()) {
        thread_.join();
    }
}

void WsProxyClient::send(const std::string& text)
{
    if (!connected_.load(std::memory_order_acquire)) {
        return;
    }
    auto self = shared_from_this();
    net::post(ioc_, [self, text]() {
        if (!self->connected_.load(std::memory_order_acquire)) {
            return;
        }
        const bool writing = !self->write_queue_.empty();
        if (self->write_queue_.size() >= WsProxyClient::kMaxPendingMessages) {
            Logger::warn("WsProxyClient backpressure: closing proxy");
            beast::error_code ec;
            self->ws_.close(websocket::close_reason("proxy_backpressure"), ec);
            self->connected_.store(false, std::memory_order_release);
            return;
        }
        self->write_queue_.push_back(text);
        if (!writing) {
            self->do_write();
        }
    });
}

void WsProxyClient::do_read()
{
    ws_.async_read(
        buffer_,
        [self = shared_from_this()](beast::error_code ec, std::size_t bytes) {
            self->on_read(ec, bytes);
        });
}

void WsProxyClient::on_read(beast::error_code ec, std::size_t /*bytes_transferred*/)
{
    if (ec == websocket::error::closed) {
        connected_.store(false, std::memory_order_release);
        return;
    }
    if (ec) {
        Logger::error("WsProxyClient read error: " + ec.message());
        connected_.store(false, std::memory_order_release);
        if (auto owner = owner_.lock()) {
            owner->send_text(R"({"type":"proxy_closed","reason":"read_error"})");
        }
        return;
    }

    const std::string payload = beast::buffers_to_string(buffer_.data());
    buffer_.consume(buffer_.size());
    if (auto owner = owner_.lock()) {
        owner->send(payload);
    }
    do_read();
}

void WsProxyClient::do_write()
{
    ws_.text(true);
    ws_.async_write(
        net::buffer(write_queue_.front()),
        [self = shared_from_this()](beast::error_code ec, std::size_t /*bytes_written*/) {
            if (ec) {
                Logger::error("WsProxyClient send error: " + ec.message());
                self->connected_.store(false, std::memory_order_release);
                return;
            }
            self->write_queue_.pop_front();
            if (!self->write_queue_.empty()) {
                self->do_write();
            }
        });
}

} // namespace taskhub
