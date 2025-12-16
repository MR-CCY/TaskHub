#include "remote_strategy.h"
#include <json.hpp>
#include <httplib.h>
#include "worker/worker_registry.h"
#include "worker/worker_info.h"
#include "runner/task_config.h"
#include "runner/task_result.h"
#include "core/logger.h"
#include "core/log_manager.h"
#include "core/log_record.h"
#include <chrono>
#include "core/ws_log_streamer.h"
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

        // TODO（M11 使用）：向远程 Worker 发送任务：
        //   - 根据 cfg.metadata / queue / priority 选择 Worker
        //   - 通过 HTTP / RPC / 消息队列投递任务
        //   - 等待 Worker 回调 / 查询状态
        //   - 支持取消时，发 cancel 请求给 Worker
        //
        // 目前你可以先占位，返回 Failed 或 NotImplemented
         // 1. 选择队列（如果没填 queue，用默认的）
        std::string queue = cfg.queue.empty() ? "default" : cfg.queue;
        // M12: start event
        core::emitEvent(cfg,
                  LogLevel::Info,
                  "RemoteExecution: dispatch start",
                  0,
                  1,
                  base_remote_fields(cfg, queue, /*workerId*/"", /*host*/"", /*port*/0));
        // 2. 从 WorkerRegistry 里选一个 Worker
        auto optWorker = worker::WorkerRegistry::instance().pickWorkerForQueue(queue);
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

        auto innerCfg = makeInnerTask(cfg);
        json jReq = buildRequestJson(innerCfg);
        const std::string body = jReq.dump();
           // 3. 构造 HTTP 客户端
        httplib::Client cli(worker.host, worker.port);
        // connect/write 给一个合理上限；read 尽量受 deadline 约束
        cli.set_connection_timeout(3, 0); // 3s connect
        cli.set_write_timeout(10, 0);     // 10s send

        if (deadline != Deadline::max()) {
            const auto now = SteadyClock::now();
            if (deadline <= now) {
                result.status  = core::TaskStatus::Timeout;
                result.message = "RemoteExecution: timeout before http call";
                Logger::error(result.message);

                auto f = base_remote_fields(cfg, queue, worker.id, worker.host, worker.port);
                f["status"] = std::to_string(static_cast<int>(result.status));
                 core::emitEvent(cfg,
                          LogLevel::Warn,
                          "RemoteExecution: dispatch end (timeout before http)",
                          result.durationMs,
                          result.attempt,
                          f);
                core::emitEvent(cfg,
                          LogLevel::Warn,
                          result.message,
                          result.durationMs,
                          result.attempt,
                          f);
                return result;
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
                    " -> " + worker.host + ":" + std::to_string(worker.port) +
                    ", workerId=" + worker.id +
                    ", taskId=" + cfg.id.value);
        const auto start = SteadyClock::now();
        auto resp = cli.Post(kWorkerExecutePath, body, "application/json");
        const auto end = SteadyClock::now();
        result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        {
            auto f = base_remote_fields(cfg, queue, worker.id, worker.host, worker.port);
            f["duration_ms"] = std::to_string(result.durationMs);
            core::emitEvent(cfg,
                      LogLevel::Debug,
                      "RemoteExecution: http call finished",
                      result.durationMs,
                      result.attempt,
                      f);
             ws::WsLogStreamer::instance().pushTaskEvent(
                        cfg.id.value,
                        "remote_dispatch",
                        {
                            {"worker_id", worker.id},
                            {"worker_host", worker.host},
                            {"worker_port", std::to_string(worker.port)}
                        }
                    );
        }

        if (!resp) {
            // best-effort：根据 cancel/deadline 推断失败类型
            if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) {
                result.status  = core::TaskStatus::Canceled;
                result.message = "RemoteExecution: canceled during http call";
            } else if (deadline != Deadline::max() && SteadyClock::now() >= deadline) {
                result.status  = core::TaskStatus::Timeout;
                result.message = "RemoteExecution: timeout during http call";
            } else {
                result.status  = core::TaskStatus::Failed;
                result.message = "RemoteExecution: HTTP failed (no response)";
            }
            Logger::error(result.message);

            
            auto f = base_remote_fields(cfg, queue, worker.id, worker.host, worker.port);
            f["status"] = core::TaskStatusTypetoString(result.status);
            f["duration_ms"] = std::to_string(result.durationMs);
            core::emitEvent(cfg,
                      LogLevel::Error,
                      "RemoteExecution: dispatch end (http no response)",
                      result.durationMs,
                      result.attempt,
                      f);
            core::emitEvent(cfg,
                      LogLevel::Error,
                      result.message,
                      result.durationMs,
                      result.attempt,
                      f);
            std::string reason;
            if (result.status == core::TaskStatus::Canceled) {
                reason = "canceled";
            } else if (result.status == core::TaskStatus::Timeout) {
                reason = "timeout";
            } else {
                reason = "no_response";
            }
            ws::WsLogStreamer::instance().pushTaskEvent(
                cfg.id.value,
                "remote_failed",
                {
                    {"reason", reason},
                    {"worker_id", worker.id},
                    {"worker_host", worker.host},
                    {"worker_port", std::to_string(worker.port)}
                }
            );
            return result;
        }
    
        if (resp->status != 200) {
            result.status     = core::TaskStatus::Failed;
            result.message    = "RemoteExecution: HTTP status=" + std::to_string(resp->status);
            result.stderrData = resp->body; // 把 body 当错误信息
            Logger::error(result.message);

            // M12: push error body into stderr buffer
            push_lines_to_buffer(cfg.id, /*isStdout*/false, result.stderrData);

            auto f = base_remote_fields(cfg, queue, worker.id, worker.host, worker.port);
            f["http_status"] = std::to_string(resp->status);
            f["status"] = TaskStatusTypetoString(result.status);
            f["duration_ms"] = std::to_string(result.durationMs);
            core::emitEvent(cfg,
                      LogLevel::Error,
                      "RemoteExecution: dispatch end (http non-200)",
                      result.durationMs,
                      result.attempt,
                      f);
            core::emitEvent(cfg,
                      LogLevel::Error,
                      result.message,
                      result.durationMs,
                      result.attempt,
                      f);
            ws::WsLogStreamer::instance().pushTaskEvent(
                cfg.id.value,
                "remote_failed",
                {
                    {"reason", "http_status"},
                    {"http_status", std::to_string(resp->status)},
                    {"worker_id", worker.id},
                    {"worker_host", worker.host},
                    {"worker_port", std::to_string(worker.port)}
                }
            );
            return result;
        }
    
        // 7) parse json（宽松解析，避免异常）
        json jResp = json::parse(resp->body, nullptr, false);
        if (jResp.is_discarded()) {
            result.status     = core::TaskStatus::Failed;
            result.message    = "RemoteExecution: invalid JSON response";
            result.stderrData = resp->body;
            Logger::error(result.message);

            push_lines_to_buffer(cfg.id, /*isStdout*/false, result.stderrData);

            auto f = base_remote_fields(cfg, queue, worker.id, worker.host, worker.port);
            f["status"] = std::to_string(static_cast<int>(result.status));
            f["duration_ms"] = std::to_string(result.durationMs);
            core::emitEvent(cfg,
                      LogLevel::Error,
                      "RemoteExecution: dispatch end (invalid json)",
                      result.durationMs,
                      result.attempt,
                      f);
            core::emitEvent(cfg,
                      LogLevel::Error,
                      result.message,
                      result.durationMs,
                      result.attempt,
                      f);
            ws::WsLogStreamer::instance().pushTaskEvent(
                cfg.id.value,
                "remote_failed",
                {
                    {"reason", "invalid_json"},
                    {"worker_id", worker.id},
                    {"worker_host", worker.host},
                    {"worker_port", std::to_string(worker.port)}
                }
            );
            return result;
        }
        Logger::debug("RemoteExecution: got response: " + resp->body);  
        // 8) map to TaskResult
        core::TaskResult parsed = core::parseResultJson(jResp);

        // M12: push stdout/stderr to LogManager buffer (line-by-line)
        if (cfg.captureOutput) {
            push_lines_to_buffer(cfg.id, /*isStdout*/true,  parsed.stdoutData);
            push_lines_to_buffer(cfg.id, /*isStdout*/false, parsed.stderrData);
        }

        // 9) keep worker info (avoid losing them if parse doesn't set)
        parsed.workerId    = worker.id;
        parsed.workerHost  = worker.host;
        parsed.workerPort  = worker.port;
        // 若 worker 侧没返回 duration，这里用我们本次 HTTP 调用耗时兜底
        if (parsed.durationMs <= 0) {
            parsed.durationMs = result.durationMs;
        }

        {
            auto f = base_remote_fields(cfg, queue, worker.id, worker.host, worker.port);
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

        return parsed;
    }
}