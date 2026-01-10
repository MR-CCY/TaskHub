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
#include <cctype>
#include <optional>
#include "ws/ws_log_streamer.h"
#include "core/config.h"
#include "utils/http_params.h"
using json = nlohmann::json;

namespace {
using Fields = std::unordered_map<std::string, std::string>;

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

inline Fields base_remote_fields(const taskhub::core::TaskConfig& cfg, const std::string& queue,const std::string& workerId,const std::string& workerHost,int workerPort)
{
    Fields f;
    f["exec_type"] = taskhub::core::TaskExecTypetoString(cfg.execType);
    f["queue"] = queue;
    if (!workerId.empty())   f["worker_id"] = workerId;
    if (!workerHost.empty()) f["worker_host"] = workerHost;
    if (workerPort > 0)      f["worker_port"] = std::to_string(workerPort);
    return f;
}

inline void emit_dispatch_end_with_message(const taskhub::core::TaskConfig& cfg,taskhub::LogLevel level,const std::string& headline,long long durationMs,int attempt,const Fields& fields,const std::string& message)
{
    taskhub::core::emitEvent(cfg, level, headline, durationMs, attempt, fields);
    taskhub::core::emitEvent(cfg, level, message, durationMs, attempt, fields);
}

inline json worker_extra_json(const taskhub::worker::WorkerInfo& workerInfo)
{
    return {
        {"worker_id", workerInfo.id},
        {"worker_host", workerInfo.host},
        {"worker_port", std::to_string(workerInfo.port)},
    };
}

inline void push_ws_task_event(const std::string& taskId,const std::string& event,const taskhub::worker::WorkerInfo& workerInfo,const json& extra = {})
{
    json merged = worker_extra_json(workerInfo);
    for (auto it = extra.begin(); it != extra.end(); ++it) {
        merged[it.key()] = it.value();
    }
    taskhub::ws::WsLogStreamer::instance().pushTaskEvent(taskId, event, merged);
}

inline std::optional<taskhub::core::TaskResult> pre_dispatch_abort(const taskhub::core::ExecutionContext& ctx)
{
    if (ctx.isCanceled()) {
        return ctx.canceled("RemoteExecution: canceled before dispatch");
    }
    if (ctx.isTimeout()) {
        return ctx.timeout("RemoteExecution: timeout before dispatch");
    }
    return std::nullopt;
}

inline int read_dispatch_max_retries() {
    int v = taskhub::Config::instance().get("dispatch.max_retries", 2);
    if (v < 0) {
        return 0;
    }
    return v;
}

inline taskhub::core::TaskResult build_dispatch_ack_result(const json& payload) {
    taskhub::core::TaskResult r;
    
    // 如果包含 run_id，说明分派成功（由 worker_execute 处理）
    bool ok = payload.value("ok", false);
    if (!ok && payload.contains("run_id") && payload["run_id"].is_string()) {
        ok = true;
    }

    r.status = ok ? taskhub::core::TaskStatus::Success : taskhub::core::TaskStatus::Failed;
    r.exitCode = ok ? 0 : 1;
    if (payload.contains("message") && payload["message"].is_string()) {
        r.message = payload["message"].get<std::string>();
    } else if (payload.contains("error") && payload["error"].is_string()) {
        r.message = payload["error"].get<std::string>();
    } else {
        r.message = ok ? "remote dispatch ok" : "remote dispatch failed";
    }

    if (payload.contains("run_id") && payload["run_id"].is_string()) {
        r.metadata["run_id"] = payload["run_id"].get<std::string>();
    }

    if (payload.contains("task_ids")) {
        r.metadata["task_ids"] = payload["task_ids"].dump();
    }
    return r;
}

} // namespace

namespace taskhub::runner {
    core::TaskResult RemoteExecutionStrategy::execute(core::ExecutionContext& ctx)
    {
        core::TaskResult result;
        const auto& cfg = ctx.config;

        // 0) 取消/超时前置检查
        if (auto abort = pre_dispatch_abort(ctx); abort.has_value()) {
            return *abort;
        }

        // 1. 选择队列（如果没填 queue，用默认的）
        std::string queue = cfg.queue.empty() ? "default" : cfg.queue;
        // M12: start event
        core::emitEvent(cfg,LogLevel::Info,"RemoteExecution: dispatch start",0,1,base_remote_fields(cfg, queue, "", "", 0));

        json payloadJson;
        std::string parseErr;
        if (!core::extractDagBody(cfg, payloadJson, parseErr)) {
            result.status = core::TaskStatus::Failed;
            result.message = "RemoteExecution: " + parseErr;
            Logger::error(result.message);
            return result;
        }

        const int maxRetries = read_dispatch_max_retries();
        const int maxAttempts = 1 + maxRetries;
        const auto cooldown = std::chrono::seconds(5);

        for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
            if (auto abort = pre_dispatch_abort(ctx); abort.has_value()) {
                return *abort;
            }

            auto optWorker = worker::ServerWorkerRegistry::instance().pickWorkerForQueue(queue);
            if (!optWorker.has_value()) {
                result.status  = core::TaskStatus::Failed;
                result.message = "RemoteExecution: no available worker for queue=" + queue;
                Logger::error(result.message);

                // M12: end event (failed)
                auto f = base_remote_fields(cfg, queue, "", "", 0);
                f["status"] = std::to_string(static_cast<int>(result.status));
                emit_dispatch_end_with_message(cfg,LogLevel::Error,"RemoteExecution: dispatch end (no worker)",result.durationMs,result.attempt,f,result.message);
                return result;
            }

            worker::WorkerInfo worker = *optWorker; // 拷贝一份，避免锁问题
            result.workerId   = worker.id;
            result.workerHost = worker.host;
            result.workerPort = worker.port;
            {
                auto f = base_remote_fields(cfg, queue, worker.id, worker.host, worker.port);
                core::emitEvent(cfg, LogLevel::Info, "RemoteExecution: picked worker",result.durationMs,result.attempt,f);
            }

            DispatchAttemptResult attemptResult = dispatch_once(worker,ctx,queue,payloadJson);
            result = attemptResult.result;
            if (attemptResult.retryable) {
                worker::ServerWorkerRegistry::instance().markDispatchFailure(worker.id, cooldown);
            }
            if (!attemptResult.retryable || attempt == maxAttempts) {
                return result;
            }

            Logger::warn("RemoteExecution: dispatch retry attempt=" +std::to_string(attempt + 1) + "/" + std::to_string(maxAttempts) +", worker_id=" + worker.id +", reason=" + (attemptResult.retryReason.empty() ? "unknown" : attemptResult.retryReason));
        }

        return result;
    }
    RemoteExecutionStrategy::DispatchAttemptResult RemoteExecutionStrategy::dispatch_once(const worker::WorkerInfo &workerInfo, const core::ExecutionContext& ctx, const std::string& queue,const json& payloadJson)
    {
            DispatchAttemptResult out;
            const auto& cfg = ctx.config;
            auto cancelFlag = ctx.getCancelFlag();
            auto deadline = ctx.getDeadline();

            json payloadToSend = payloadJson;
	            // 注入 run_id（用于分布式 DAG 跨 worker 统一查询/透传）
            if (!cfg.id.runId.empty()) {
                const bool missing =
                    !payloadToSend.contains("run_id") ||
                    !payloadToSend["run_id"].is_string() ||
                    payloadToSend["run_id"].get<std::string>().empty();
                if (missing) {
                    payloadToSend["run_id"] = cfg.id.runId;
                }
            }
            // jReq["payload"] = payloadToSend;

            const std::string body = payloadToSend.dump();
            // 3. 构造 HTTP 客户端
            httplib::Client cli(workerInfo.host, workerInfo.port);
            // connect/write 给一个合理上限；read 尽量受 deadline 约束
            cli.set_connection_timeout(3, 0); // 3s connect
            cli.set_write_timeout(10, 0);     // 10s send

            core::TaskResult attempt;
            attempt.workerId   = workerInfo.id;
            attempt.workerHost = workerInfo.host;
            attempt.workerPort = workerInfo.port;

            if (deadline != Deadline::max()) {
                const auto now = SteadyClock::now();
                if (deadline <= now) {
                    attempt.status  = core::TaskStatus::Timeout;
                    attempt.message = "RemoteExecution: timeout before http call";
                    Logger::error(attempt.message);

                    auto f = base_remote_fields(cfg, queue, workerInfo.id, workerInfo.host, workerInfo.port);
                    f["status"] = std::to_string(static_cast<int>(attempt.status));
                    emit_dispatch_end_with_message(cfg,LogLevel::Warn,"RemoteExecution: dispatch end (timeout before http)",attempt.durationMs,attempt.attempt,f,attempt.message);

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

            Logger::info(std::string("RemoteExecution: POST ") + kWorkerExecutePath + " -> " + workerInfo.host + ":" + std::to_string(workerInfo.port) +", workerId=" + workerInfo.id + ", taskId=" + cfg.id.value);
            const auto start = SteadyClock::now();
            auto resp = cli.Post(kWorkerExecutePath, body, "application/json");
            const auto end = SteadyClock::now();
            attempt.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            {
                auto f = base_remote_fields(cfg, queue, workerInfo.id, workerInfo.host, workerInfo.port);
                f["duration_ms"] = std::to_string(attempt.durationMs);
                core::emitEvent(cfg,LogLevel::Debug,"RemoteExecution: http call finished",attempt.durationMs,attempt.attempt,f);
                push_ws_task_event(cfg.id.value, "remote_dispatch", workerInfo);
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
                    out.retryReason = http_params::http_error_to_string(resp.error());
                }
                Logger::error(attempt.message);

                auto f = base_remote_fields(cfg, queue, workerInfo.id, workerInfo.host, workerInfo.port);
                f["status"] = core::TaskStatusTypetoString(attempt.status);
                f["duration_ms"] = std::to_string(attempt.durationMs);

                emit_dispatch_end_with_message(cfg,LogLevel::Error,"RemoteExecution: dispatch end (http no response)",attempt.durationMs,attempt.attempt,f,attempt.message);

                std::string reason;
                if (attempt.status == core::TaskStatus::Canceled) {
                    reason = "canceled";
                } else if (attempt.status == core::TaskStatus::Timeout) {
                    reason = "timeout";
                } else {
                    reason = "no_response";
                }
                push_ws_task_event(cfg.id.value, "remote_failed", workerInfo, {{"reason", reason}});
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
                emit_dispatch_end_with_message(cfg,LogLevel::Error,"RemoteExecution: dispatch end (http non-200)",attempt.durationMs,attempt.attempt,f,attempt.message);
                push_ws_task_event(cfg.id.value,"remote_failed",workerInfo,{{"reason", "http_status"}, {"http_status", std::to_string(resp->status)}});
                if (http_params::is_retryable_http_status(resp->status)) {
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

                push_lines_to_buffer(cfg.id, false, attempt.stderrData);

                auto f = base_remote_fields(cfg, queue, workerInfo.id, workerInfo.host, workerInfo.port);
                f["status"] = std::to_string(static_cast<int>(attempt.status));
                f["duration_ms"] = std::to_string(attempt.durationMs);
                emit_dispatch_end_with_message(cfg,LogLevel::Error,"RemoteExecution: dispatch end (invalid json)",attempt.durationMs,attempt.attempt,f,attempt.message);
                push_ws_task_event(cfg.id.value, "remote_failed", workerInfo, {{"reason", "invalid_json"}});
                out.result = attempt;
                return out;
            }
            Logger::debug("RemoteExecution: got response: " + resp->body);
            // 8) map to TaskResult
            const json* payload = &jResp;
            if (jResp.contains("data") && jResp["data"].is_object()) {
                payload = &jResp["data"];
            }
        
            core::TaskResult parsed= build_dispatch_ack_result(*payload);

            // M12: push stdout/stderr to LogManager buffer (line-by-line)
            if (cfg.captureOutput) {
                push_lines_to_buffer(cfg.id,true, parsed.stdoutData);
                push_lines_to_buffer(cfg.id,false, parsed.stderrData);
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
                core::emitEvent(cfg,parsed.ok() ? LogLevel::Info : LogLevel::Warn,"RemoteExecution: dispatch end",parsed.durationMs,parsed.attempt,f);
            }

            out.result = parsed;
            return out;
    }
}
