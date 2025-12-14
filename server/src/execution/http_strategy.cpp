#include "core/log_manager.h"
#include "core/log_record.h"
#include "http_strategy.h"
#include "httplib.h"
namespace taskhub::runner {

namespace {
inline void push_text_as_stdout(const taskhub::core::TaskConfig& cfg, const std::string& text)
{
    if (!cfg.captureOutput) return;
    if (text.empty()) return;

    // Split by '\n' and push line-by-line
    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string::npos) {
            taskhub::core::LogManager::instance().stdoutLine(cfg.id, text.substr(start));
            break;
        }
        taskhub::core::LogManager::instance().stdoutLine(cfg.id, text.substr(start, end - start));
        start = end + 1;
    }
}

} // namespace

    /// 执行HTTP调用任务
    /// @param cfg 任务配置信息，包含URL和其他执行参数
    /// @param cancelFlag 取消标志，用于检查任务是否被取消
    /// @param deadline 任务截止时间点，用于超时控制
    /// @return 返回任务执行结果，包括执行状态、消息和执行时长
    core::TaskResult HttpExecutionStrategy::execute(const core::TaskConfig &cfg, std::atomic_bool *cancelFlag, Deadline deadline)
    {
        core::TaskResult r;
        const auto start = SteadyClock::now();
        core::emitEvent(cfg, LogLevel::Info, "http dispatch start", 0);
        if (cfg.execCommand.empty()) {
            r.status  = core::TaskStatus::Failed;
            r.message = "TaskRunner：空的http url";
            core::emitEvent(cfg, LogLevel::Error, "http invalid: empty url", 0);
            return r;
        }
    
        if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) {
            r.status  = core::TaskStatus::Canceled;
            r.message = "TaskRunner：在http调用之前取消";
            core::emitEvent(cfg, LogLevel::Warn, "http canceled before call", 0);
            return r;
        }

        // 粗略解析 URL，支持 http://host[:port]/path 形式
        std::string scheme = "http";
        std::string host;
        std::string path = "/";
        int port         = 80;

        const auto schemePos = cfg.execCommand.find("://");
        std::string rest     = cfg.execCommand;
        if (schemePos != std::string::npos) {
            scheme = cfg.execCommand.substr(0, schemePos);
            rest   = cfg.execCommand.substr(schemePos + 3);
        }

        const auto pathPos = rest.find('/');
        const std::string hostPort = pathPos == std::string::npos ? rest : rest.substr(0, pathPos);
        if (pathPos != std::string::npos) {
            path = rest.substr(pathPos);
        }
    
        const auto colonPos = hostPort.find(':');
        if (colonPos != std::string::npos) {
            host = hostPort.substr(0, colonPos);
            port = std::atoi(hostPort.substr(colonPos + 1).c_str());
        } else {
            host = hostPort;
            port = scheme == "https" ? 443 : 80;
        }
    
        if (scheme != "http" || host.empty()) {
            r.status  = core::TaskStatus::Failed;
            r.message = "TaskRunner：不受支持或无效的URL";
            core::emitEvent(cfg, LogLevel::Error, "http invalid: unsupported url", 0);
            return r;
        }

        core::emitEvent(cfg, LogLevel::Debug, "http parsed url", 0, 1, {
            {"scheme", scheme},
            {"host", host},
            {"port", std::to_string(port)},
            {"path", path}
        });

        httplib::Client cli(host, port);
        cli.set_keep_alive(false);

        if (deadline != Deadline::max()) {
            const auto now = SteadyClock::now();
            if (deadline <= now) {
                r.status  = core::TaskStatus::Timeout;
                r.message = "TaskRunner: timeout before http call";
                core::emitEvent(cfg, LogLevel::Warn, "http timeout before call", 0);
                return r;
            }

            // 剩余时间：用于设置 httplib 的连接/读写超时
            // 说明：cpp-httplib 的 timeout 只能在调用前设置，无法真正中断正在阻塞的请求。
            //      因此这里将 deadline 映射为网络层 timeout，作为“硬超时”。
            auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
            if (remain.count() <= 0) {
                remain = std::chrono::milliseconds(1);
            }

            const int sec  = static_cast<int>(remain.count() / 1000);
            const int usec = static_cast<int>((remain.count() % 1000) * 1000);

            cli.set_connection_timeout(sec, usec);
            cli.set_read_timeout(sec, usec);
            cli.set_write_timeout(sec, usec);
        }

        httplib::Params params;
        for (const auto &kv : cfg.execParams) {
            params.emplace(kv.first, kv.second);
        }

        core::emitEvent(cfg, LogLevel::Info, "http request start", 0, 1, {
            {"method", params.empty() ? "GET" : "POST"}
        });

        auto res = params.empty() ? cli.Get(path) : cli.Post(path, params);

        const auto end = SteadyClock::now();
        r.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        if (res) {
            core::emitEvent(cfg, LogLevel::Debug, "http response received", r.durationMs, 1, {
                {"status_code", std::to_string(res->status)},
                {"body_bytes", std::to_string(res->body.size())}
            });
        } else {
            core::emitEvent(cfg, LogLevel::Warn, "http no response", r.durationMs);
        }

        if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) {
            r.status  = core::TaskStatus::Canceled;
            r.message = "TaskRunner：在http调用期间取消";
        } else if (!res) {
            r.status  = core::TaskStatus::Failed;
            r.message = "TaskRunner: http 错误 " + httplib::to_string(res.error());
            r.stderrData = httplib::to_string(res.error());
            if (cfg.captureOutput) {
                taskhub::core::LogManager::instance().stderrLine(cfg.id, r.stderrData);
            }
        } else if (res->status >= 200 && res->status < 300) {
            r.status = core::TaskStatus::Success;
            // Capture response body as stdout for now
            if (res) {
                r.stdoutData = res->body;
                push_text_as_stdout(cfg, r.stdoutData);
            }
        } else {
            r.status  = core::TaskStatus::Failed;
            r.message = "TaskRunner: http 状态 " + std::to_string(res->status);
            if (res) {
                r.stdoutData = res->body;
                push_text_as_stdout(cfg, r.stdoutData);
            }
        }

        core::emitEvent(cfg,
                        (r.status == core::TaskStatus::Success) ? LogLevel::Info
                                                               : LogLevel::Warn,
                        "http dispatch end", r.durationMs, 1,
                        {
                            {"task_status", std::to_string(static_cast<int>(r.status))},
                            {"message", r.message}
                        });

        Logger::info("HttpExecutionStrategy::execute, id=" + cfg.id.value +
                    ", name=" + cfg.name +
                    ", status=" + std::to_string(static_cast<int>(r.status)) +
                    ", message=" + r.message +
                    ", duration=" + std::to_string(r.durationMs) + "ms");
        return r;
    }
}