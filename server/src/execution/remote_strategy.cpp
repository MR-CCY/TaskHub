#include "remote_strategy.h"
#include <json.hpp>
#include <httplib.h>
#include "worker/server_worker_registry.h"
#include "worker/worker_info.h"
#include "runner/task_config.h"
#include "runner/task_result.h"
#include "log/logger.h"
#include "log/log_manager.h"
#include "log/log_record.h"
#include <chrono>
#include "ws/ws_log_streamer.h"
#include "core/config.h"
using json = nlohmann::json;

namespace {

inline void push_lines_to_buffer(const taskhub::core::TaskId& taskId,
                                 bool isStdout,
                                 const std::string& text)
{
    if (text.empty()) return;

    // Split by '\n' and write line-by-line.
    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t end = text.find('\n', start);
        std::string line;
        if (end == std::string::npos) {
            line = text.substr(start);
            start = text.size() + 1;
        } else {
            line = text.substr(start, end - start);
            start = end + 1;
        }

        // Avoid writing an extra empty line caused by trailing '\n'
        if (line.empty() && start > text.size()) {
            break;
        }

        if (isStdout) {
            taskhub::core::LogManager::instance().stdoutLine(taskId, line);
        } else {
            taskhub::core::LogManager::instance().stderrLine(taskId, line);
        }
    }
}

inline std::unordered_map<std::string, std::string>
base_remote_fields(const taskhub::core::TaskConfig& cfg,
                   const std::string& queue,
                   const std::string& workerId,
                   const std::string& workerHost,
                   int workerPort)
{
    std::unordered_map<std::string, std::string> f;
    f["exec_type"] = taskhub::core::TaskExecTypetoString(cfg.execType);
    f["queue"] = queue;
    if (!workerId.empty())   f["worker_id"] = workerId;
    if (!workerHost.empty()) f["worker_host"] = workerHost;
    if (workerPort > 0)      f["worker_port"] = std::to_string(workerPort);
    return f;
}

struct DispatchAttemptResult {
    taskhub::core::TaskResult result;
    bool retryable{false};
    std::string retryReason;
};

inline int read_dispatch_max_retries() {
    int v = taskhub::Config::instance().get("dispatch.max_retries", 2);
    if (v < 0) {
        return 0;
    }
    return v;
}

inline bool is_retryable_http_status(int status) {
    return status >= 500 && status <= 599;
}

inline std::string http_error_to_string(httplib::Error err) {
    switch (err) {
        case httplib::Error::Connection:
            return "connection";
        case httplib::Error::Read:
            return "read";
        case httplib::Error::Write:
            return "write";
        case httplib::Error::ConnectionTimeout:
            return "timeout";
        case httplib::Error::Canceled:
            return "canceled";
        case httplib::Error::SSLConnection:
            return "ssl_connection";
        case httplib::Error::SSLServerVerification:
            return "ssl_verify";
        case httplib::Error::SSLServerHostnameVerification:
            return "ssl_hostname_verify";
        default:
            return "error_" + std::to_string(static_cast<int>(err));
    }
}

} // namespace

namespace taskhub::runner {
    core::TaskResult RemoteExecutionStrategy::execute(const core::TaskConfig &cfg, std::atomic_bool *cancelFlag, Deadline deadline)
    {
        core::TaskResult result;

        // 0) 取消/超时前置检查
        if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) {
            result.status  = core::TaskStatus::Canceled;
            result.message = "RemoteExecution: canceled before dispatch";
            return result;
        }
        if (deadline != Deadline::max() && SteadyClock::now() >= deadline) {
            result.status  = core::TaskStatus::Timeout;
            result.message = "RemoteExecution: timeout before dispatch";
            return result;
        }

        // 1. 选择队列（如果没填 queue，用默认的）
        std::string queue = cfg.queue.empty() ? "default" : cfg.queue;
        // M12: start event
        core::emitEvent(cfg,
                  LogLevel::Info,
                  "RemoteExecution: dispatch start",
                  0,
                  1,
                  base_remote_fields(cfg, queue, /*workerId*/"", /*host*/"", /*port*/0));

        const int maxRetries = read_dispatch_max_retries();
        const int maxAttempts = 1 + maxRetries;
        const auto cooldown = std::chrono::seconds(5);

        auto dispatch_once = [&](const worker::WorkerInfo& workerInfo) -> DispatchAttemptResult {
            DispatchAttemptResult out;
            core::TaskResult attempt;
            attempt.workerId   = workerInfo.id;
            attempt.workerHost = workerInfo.host;
            attempt.workerPort = workerInfo.port;

            auto innerCfg = makeInnerTask(cfg);
            json jReq = buildRequestJson(innerCfg);
            const std::string body = jReq.dump();
            // 3. 构造 HTTP 客户端
            httplib::Client cli(workerInfo.host, workerInfo.port);
            // connect/write 给一个合理上限；read 尽量受 deadline 约束
            cli.set_connection_timeout(3, 0); // 3s connect
            cli.set_write_timeout(10, 0);     // 10s send

            if (deadline != Deadline::max()) {
                const auto now = SteadyClock::now();
                if (deadline <= now) {
                    attempt.status  = core::TaskStatus::Timeout;
                    attempt.message = "RemoteExecution: timeout before http call";
                    Logger::error(attempt.message);

                    auto f = base_remote_fields(cfg, queue, workerInfo.id, workerInfo.host, workerInfo.port);
                    f["status"] = std::to_string(static_cast<int>(attempt.status));
                    core::emitEvent(cfg,
                              LogLevel::Warn,
                              "RemoteExecution: dispatch end (timeout before http)",
                              attempt.durationMs,
                              attempt.attempt,
                              f);
                    core::emitEvent(cfg,
                              LogLevel::Warn,
                              attempt.message,
                              attempt.durationMs,
                              attempt.attempt,
                              f);
                    out.result = attempt;
                    return out;
                }
                auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
                if (remain.count() <= 0) {
                    remain = std::chrono::milliseconds(1);
                }
                const int sec  = static_cast<int>(remain.count() / 1000);
                const int usec = static_cast<int>((remain.count() % 1000) * 1000);
                cli.set_read_timeout(sec, usec);
            } else {
                cli.set_read_timeout(60, 0); // 无 deadline 时兜底
            }

            // 4. 发送请求
            static const char* kWorkerExecutePath = "/api/worker/execute";

            Logger::info(std::string("RemoteExecution: POST ") + kWorkerExecutePath +
                        " -> " + workerInfo.host + ":" + std::to_string(workerInfo.port) +
                        ", workerId=" + workerInfo.id +
                        ", taskId=" + cfg.id.value);
            const auto start = SteadyClock::now();
            auto resp = cli.Post(kWorkerExecutePath, body, "application/json");
            const auto end = SteadyClock::now();
            attempt.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            {
                auto f = base_remote_fields(cfg, queue, workerInfo.id, workerInfo.host, workerInfo.port);
                f["duration_ms"] = std::to_string(attempt.durationMs);
                core::emitEvent(cfg,
                          LogLevel::Debug,
                          "RemoteExecution: http call finished",
                          attempt.durationMs,
                          attempt.attempt,
                          f);
                ws::WsLogStreamer::instance().pushTaskEvent(
                            cfg.id.value,
                            "remote_dispatch",
                            {
                                {"worker_id", workerInfo.id},
                                {"worker_host", workerInfo.host},
                                {"worker_port", std::to_string(workerInfo.port)}
                            }
                        );
            }

            if (!resp) {
                // best-effort：根据 cancel/deadline 推断失败类型
                if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) {
                    attempt.status  = core::TaskStatus::Canceled;
                    attempt.message = "RemoteExecution: canceled during http call";
                } else if (deadline != Deadline::max() && SteadyClock::now() >= deadline) {
                    attempt.status  = core::TaskStatus::Timeout;
                    attempt.message = "RemoteExecution: timeout during http call";
                } else {
                    attempt.status  = core::TaskStatus::Failed;
                    attempt.message = "RemoteExecution: HTTP failed (no response)";
                    out.retryable = true;
                    out.retryReason = http_error_to_string(resp.error());
                }
                Logger::error(attempt.message);

                auto f = base_remote_fields(cfg, queue, workerInfo.id, workerInfo.host, workerInfo.port);
                f["status"] = core::TaskStatusTypetoString(attempt.status);
                f["duration_ms"] = std::to_string(attempt.durationMs);
                core::emitEvent(cfg,
                          LogLevel::Error,
                          "RemoteExecution: dispatch end (http no response)",
                          attempt.durationMs,
                          attempt.attempt,
                          f);
                core::emitEvent(cfg,
                          LogLevel::Error,
                          attempt.message,
                          attempt.durationMs,
                          attempt.attempt,
                          f);
                std::string reason;
                if (attempt.status == core::TaskStatus::Canceled) {
                    reason = "canceled";
                } else if (attempt.status == core::TaskStatus::Timeout) {
                    reason = "timeout";
                } else {
                    reason = "no_response";
                }
                ws::WsLogStreamer::instance().pushTaskEvent(
                    cfg.id.value,
                    "remote_failed",
                    {
                        {"reason", reason},
                        {"worker_id", workerInfo.id},
                        {"worker_host", workerInfo.host},
                        {"worker_port", std::to_string(workerInfo.port)}
                    }
                );
                out.result = attempt;
                return out;
            }

            if (resp->status < 200 || resp->status >= 300) {
                attempt.status     = core::TaskStatus::Failed;
                attempt.message    = "RemoteExecution: HTTP status=" + std::to_string(resp->status);
                attempt.stderrData = resp->body; // 把 body 当错误信息
                Logger::error(attempt.message);

                // M12: push error body into stderr buffer
                push_lines_to_buffer(cfg.id, /*isStdout*/false, attempt.stderrData);

                auto f = base_remote_fields(cfg, queue, workerInfo.id, workerInfo.host, workerInfo.port);
                f["http_status"] = std::to_string(resp->status);
                f["status"] = TaskStatusTypetoString(attempt.status);
                f["duration_ms"] = std::to_string(attempt.durationMs);
                core::emitEvent(cfg,
                          LogLevel::Error,
                          "RemoteExecution: dispatch end (http non-200)",
                          attempt.durationMs,
                          attempt.attempt,
                          f);
                core::emitEvent(cfg,
                          LogLevel::Error,
                          attempt.message,
                          attempt.durationMs,
                          attempt.attempt,
                          f);
                ws::WsLogStreamer::instance().pushTaskEvent(
                    cfg.id.value,
                    "remote_failed",
                    {
                        {"reason", "http_status"},
                        {"http_status", std::to_string(resp->status)},
                        {"worker_id", workerInfo.id},
                        {"worker_host", workerInfo.host},
                        {"worker_port", std::to_string(workerInfo.port)}
                    }
                );
                if (is_retryable_http_status(resp->status)) {
                    out.retryable = true;
                    out.retryReason = "http_" + std::to_string(resp->status);
                }
                out.result = attempt;
                return out;
            }

            // 7) parse json（宽松解析，避免异常）
            json jResp = json::parse(resp->body, nullptr, false);
            if (jResp.is_discarded()) {
                attempt.status     = core::TaskStatus::Failed;
                attempt.message    = "RemoteExecution: invalid JSON response";
                attempt.stderrData = resp->body;
                Logger::error(attempt.message);

                push_lines_to_buffer(cfg.id, /*isStdout*/false, attempt.stderrData);

                auto f = base_remote_fields(cfg, queue, workerInfo.id, workerInfo.host, workerInfo.port);
                f["status"] = std::to_string(static_cast<int>(attempt.status));
                f["duration_ms"] = std::to_string(attempt.durationMs);
                core::emitEvent(cfg,
                          LogLevel::Error,
                          "RemoteExecution: dispatch end (invalid json)",
                          attempt.durationMs,
                          attempt.attempt,
                          f);
                core::emitEvent(cfg,
                          LogLevel::Error,
                          attempt.message,
                          attempt.durationMs,
                          attempt.attempt,
                          f);
                ws::WsLogStreamer::instance().pushTaskEvent(
                    cfg.id.value,
                    "remote_failed",
                    {
                        {"reason", "invalid_json"},
                        {"worker_id", workerInfo.id},
                        {"worker_host", workerInfo.host},
                        {"worker_port", std::to_string(workerInfo.port)}
                    }
                );
                out.result = attempt;
                return out;
            }
            Logger::debug("RemoteExecution: got response: " + resp->body);
            // 8) map to TaskResult
            const json* payload = &jResp;
            if (jResp.contains("data") && jResp["data"].is_object()) {
                payload = &jResp["data"];
            }
            core::TaskResult parsed = core::parseResultJson(*payload);

            // M12: push stdout/stderr to LogManager buffer (line-by-line)
            if (cfg.captureOutput) {
                push_lines_to_buffer(cfg.id, /*isStdout*/true,  parsed.stdoutData);
                push_lines_to_buffer(cfg.id, /*isStdout*/false, parsed.stderrData);
            }

            // 9) keep worker info (avoid losing them if parse doesn't set)
            parsed.workerId    = workerInfo.id;
            parsed.workerHost  = workerInfo.host;
            parsed.workerPort  = workerInfo.port;
            // 若 worker 侧没返回 duration，这里用我们本次 HTTP 调用耗时兜底
            if (parsed.durationMs <= 0) {
                parsed.durationMs = attempt.durationMs;
            }

            {
                auto f = base_remote_fields(cfg, queue, workerInfo.id, workerInfo.host, workerInfo.port);
                f["status"] = std::to_string(static_cast<int>(parsed.status));
                f["exit_code"] = std::to_string(parsed.exitCode);
                f["duration_ms"] = std::to_string(parsed.durationMs);
                if (!parsed.message.empty()) {
                    f["message"] = parsed.message;
                }
                core::emitEvent(cfg,
                          parsed.ok() ? LogLevel::Info : LogLevel::Warn,
                          "RemoteExecution: dispatch end",
                          parsed.durationMs,
                          parsed.attempt,
                          f);
            }

            out.result = parsed;
            return out;
        };

        for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
            if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) {
                result.status  = core::TaskStatus::Canceled;
                result.message = "RemoteExecution: canceled before dispatch";
                return result;
            }
            if (deadline != Deadline::max() && SteadyClock::now() >= deadline) {
                result.status  = core::TaskStatus::Timeout;
                result.message = "RemoteExecution: timeout before dispatch";
                return result;
            }

            auto optWorker = worker::ServerWorkerRegistry::instance().pickWorkerForQueue(queue);
            if (!optWorker.has_value()) {
                result.status  = core::TaskStatus::Failed;
                result.message = "RemoteExecution: no available worker for queue=" + queue;
                Logger::error(result.message);

                // M12: end event (failed)
                auto f = base_remote_fields(cfg, queue, "", "", 0);
                f["status"] = std::to_string(static_cast<int>(result.status));
                core::emitEvent(cfg,
                          LogLevel::Error,
                          "RemoteExecution: dispatch end (no worker)",
                          result.durationMs,
                          result.attempt,
                          f);
                core::emitEvent(cfg,
                          LogLevel::Error,
                          result.message,
                          result.durationMs,
                          result.attempt,
                          f);
                return result;
            }

            worker::WorkerInfo worker = *optWorker; // 拷贝一份，避免锁问题
            result.workerId   = worker.id;
            result.workerHost = worker.host;
            result.workerPort = worker.port;
            {
                auto f = base_remote_fields(cfg, queue, worker.id, worker.host, worker.port);
                core::emitEvent(cfg,
                          LogLevel::Info,
                          "RemoteExecution: picked worker",
                          result.durationMs,
                          result.attempt,
                          f);
            }

            DispatchAttemptResult attemptResult = dispatch_once(worker);
            result = attemptResult.result;
            if (attemptResult.retryable) {
                worker::ServerWorkerRegistry::instance().markDispatchFailure(worker.id, cooldown);
            }
            if (!attemptResult.retryable || attempt == maxAttempts) {
                return result;
            }

            Logger::warn("RemoteExecution: dispatch retry attempt=" +
                        std::to_string(attempt + 1) + "/" + std::to_string(maxAttempts) +
                        ", worker_id=" + worker.id +
                        ", reason=" + (attemptResult.retryReason.empty() ? "unknown" : attemptResult.retryReason));
        }

        return result;
    }
}
