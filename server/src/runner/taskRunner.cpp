#include "taskRunner.h"
#include "core/logger.h"
#include <thread>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <exception>
#include <cstdlib>
#include "httplib.h"
#include "local_task_registry.h"
#include "execution/execution_registry.h"
#include "core/log_manager.h"
#include "core/ws_log_streamer.h"
namespace taskhub::runner {
using namespace core;

TaskRunner& TaskRunner::instance() {
    static TaskRunner instance;
    return instance;
}
TaskResult TaskRunner::run(const TaskConfig &cfg, std::atomic_bool *cancelFlag) const
{
    Logger::info("TaskRunner::run start, id=" + cfg.id.value +
                 ", name=" + cfg.name +
                 ", execType=" + std::to_string(static_cast<int>(cfg.execType)));

     const auto runStart = SteadyClock::now();

    // NOTE: if taskId is empty in some flows, you can still emit to Event stream; downstream may treat it as global.
    core::emitEvent(cfg.id, LogLevel::Info,"TaskRunner::run start, execType=" + std::to_string(static_cast<int>(cfg.execType)));
    ws::WsLogStreamer::instance().pushTaskEvent(
        cfg.id.value,
        "task_start",
        {
            {"exec_type", TaskExecTypetoString(cfg.execType)},
            {"queue", cfg.queue}
        }
    );
    TaskResult r = runWithRetry(cfg, cancelFlag);

    // ===== M12: end event (structured) =====
    core::LogRecord rec;
    rec.taskId   = cfg.id;
    rec.level    = r.ok() ? LogLevel::Info : LogLevel::Warn;
    rec.stream   = core::LogStream::Event;
    rec.message  = "TaskRunner::run end";

    // duration：优先用 TaskResult 的 durationMs，没有就用 run() 这层兜底
    const auto runEnd = SteadyClock::now();
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(runEnd - runStart).count();
    rec.durationMs = (r.durationMs > 0) ? r.durationMs : static_cast<long long>(elapsedMs);

    // attempt
    rec.attempt = r.attempt; 

    // summary fields
    rec.fields["status"]       = std::to_string(static_cast<int>(r.status));
    rec.fields["message"]      = r.message;
    rec.fields["exec_type"]    = std::to_string(static_cast<int>(cfg.execType));
    rec.fields["exit_code"]    = std::to_string(r.exitCode);
    rec.fields["attempt"]      = std::to_string(r.attempt);
    rec.fields["max_attempts"] = std::to_string(r.maxAttempts);

    // stdout/stderr：事件里别塞正文，先塞字节数（正文交给 TaskLogBuffer / stdout stream）
    rec.fields["stdout_bytes"] = std::to_string(r.stdoutData.size());
    rec.fields["stderr_bytes"] = std::to_string(r.stderrData.size());

    // remote worker info（如果有）
    if (!r.workerId.empty())   rec.fields["worker_id"]   = r.workerId;
    if (!r.workerHost.empty()) rec.fields["worker_host"] = r.workerHost;
    if (r.workerPort != 0)     rec.fields["worker_port"] = r.workerPort;

    core::LogManager::instance().emit(rec);
    ws::WsLogStreamer::instance().pushTaskEvent(
        cfg.id.value,
        "task_end",
        {
            {"status",    TaskStatusTypetoString(r.status)},
            {"message",   r.message},
            {"duration_ms", rec.durationMs},
            {"attempt",   r.attempt},
            {"max_attempts", r.maxAttempts}
        }
    );
    return r;
}

void TaskRunner::registerLocalTask(const std::string &key, LocalTaskFn fn)
{
    LocalTaskRegistry::instance().registerTask(key, std::move(fn));
}

// ... existing code ...

/// \brief 带重试机制的任务执行函数
/// \param cfg 任务配置信息，包括超时设置、重试次数、重试延迟等
/// \param externalCancelFlag 外部取消标志，当该标志被设置时任务会被取消
/// \return TaskResult 任务执行结果，包含状态、消息和其他相关信息
TaskResult TaskRunner::runWithRetry(const TaskConfig &cfg, std::atomic_bool *externalCancelFlag) const
{
    TaskResult lastResult;

    // STEP 0：计算最大尝试次数（retryCount 次重试 = retryCount + 1 次总尝试）
    int maxAttempts = cfg.retryCount + 1;
    if (maxAttempts <= 0) {
        maxAttempts = 1;
    }
    lastResult.maxAttempts= maxAttempts;

    // 基础重试间隔（<=0 时兜底为 1s）
    auto baseDelay = cfg.retryDelay;
    if (baseDelay.count() <= 0) {
        baseDelay = std::chrono::milliseconds(1000);
    }

    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        // STEP 1：本轮尝试前，先检查是否已经被外部取消
        if (externalCancelFlag &&
            externalCancelFlag->load(std::memory_order_acquire)) {
            lastResult.status  = core::TaskStatus::Canceled;
            lastResult.message = "TaskRunner: canceled before attempt";
            core::emitEvent(cfg.id, LogLevel::Warn,
                      "TaskRunner canceled before attempt, attempt=" + std::to_string(attempt + 1) + "/" + std::to_string(maxAttempts));
            return lastResult;
        }

        // STEP 2：计算本轮执行的 deadline（软超时截止时间）
        SteadyClock::time_point deadline = SteadyClock::time_point::max();
        if (cfg.hasTimeout()) {
            deadline = SteadyClock::now() + cfg.timeout;
        }

        Logger::info("TaskRunner::runWithRetry, id=" + cfg.id.value +
                     ", name=" + cfg.name +
                     ", attempt=" + std::to_string(attempt + 1) + "/" + std::to_string(maxAttempts) +
                     ", deadline=" + std::to_string(deadline.time_since_epoch().count()));
        ws::WsLogStreamer::instance().pushTaskEvent(
            cfg.id.value,
            "attempt_start",
            {
                {"attempt", attempt + 1},
                {"max_attempts", maxAttempts}
            }
        );

        // STEP 3：真正执行一次（内部已负责：超时 / 取消 / 调度策略）
        core::emitEvent(cfg.id, LogLevel::Info,
                  "Attempt start " + std::to_string(attempt + 1) + "/" + std::to_string(maxAttempts));
        lastResult = runOneAttempt(cfg, externalCancelFlag, deadline);
        core::emitEvent(cfg.id,
                  lastResult.ok() ? LogLevel::Info : LogLevel::Warn,
                  "Attempt end " + std::to_string(attempt + 1) + "/" + std::to_string(maxAttempts) +
                  ", status=" + std::to_string(static_cast<int>(lastResult.status)) +
                  ", message=" + lastResult.message);
        ws::WsLogStreamer::instance().pushTaskEvent(
            cfg.id.value,
            "attempt_end",
            {
                {"attempt", attempt + 1},
                {"status", TaskStatusTypetoString(lastResult.status)},
                {"message", lastResult.message}
            }
        );
         // 添加 attempt 字段
        lastResult.attempt = attempt + 1;
        // STEP 4：根据结果决定是否结束重试

        // 4.1 成功：直接返回
        if (lastResult.ok()) {
            return lastResult;
        }

        // 4.2 被取消或超时：通常视为“终止条件”，不再重试
        if (lastResult.status == core::TaskStatus::Canceled ||
            lastResult.status == core::TaskStatus::Timeout) {
            core::emitEvent(cfg.id, LogLevel::Warn,
                      "Stop retry due to terminal status=" + std::to_string(static_cast<int>(lastResult.status)));
            return lastResult;
        }

        // 4.3 普通失败（Failed）：如果还有机会，进入 STEP 5 做退避等待
        if (attempt + 1 >= maxAttempts) {
            // 已经是最后一次尝试，直接跳出循环，在末尾返回 lastResult
            break;
        }

        // STEP 5：在下一次重试前做退避等待，并在等待期间支持取消

        auto delay = baseDelay;
        if (cfg.retryUseExponentialBackoff) {
            // 简单指数退避：1x, 2x, 4x, ...
            delay *= (1 << attempt);
        }

        core::emitEvent(cfg.id, LogLevel::Info,
                  "Retry backoff " + std::to_string(delay.count()) + "ms before next attempt");

        auto sleepUntil = SteadyClock::now() + delay;
        while (SteadyClock::now() < sleepUntil) {
            // 等待过程中仍然允许被取消，尽快返回
            if (externalCancelFlag &&
                externalCancelFlag->load(std::memory_order_acquire)) {
                lastResult.status  = core::TaskStatus::Canceled;
                lastResult.message = "TaskRunner: canceled during retry backoff";
                core::emitEvent(cfg.id, LogLevel::Warn, "Canceled during retry backoff");
                return lastResult;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    // STEP 6：所有尝试结束，返回最后一次的结果
    // （可能是 Failed / Canceled / Timeout）
    return lastResult;
}

// ... existing code ...

TaskResult TaskRunner::runOneAttempt(const TaskConfig &cfg,
                                     std::atomic_bool *externalCancelFlag,
                                     SteadyClock::time_point deadline) const
{
    core::TaskResult result;

    // 1) 取消标记：优先用外面的，否则用内部的
    std::atomic_bool internalCancel{false};
    std::atomic_bool* effectiveCancel =
        externalCancelFlag ? externalCancelFlag : &internalCancel;

    // 2) 超时前置检查（极端情况：调用时已过期）
    if (cfg.hasTimeout()) {
        const auto now = SteadyClock::now();
        if (deadline <= now) {
            if (effectiveCancel) {
                effectiveCancel->store(true, std::memory_order_release);
            }
            result.status  = core::TaskStatus::Timeout;
            result.message = "TaskRunner: timeout before execution";
            return result;
        }
    }

    // 3) 同步执行：不再为每个任务额外启动/分离线程。
    //    超时/取消的“真正终止能力”交给各 execution strategy：
    //    - Shell(B): fork/exec + killpg
    //    - HttpCall: client timeout
    //    - Local: 任务自身需轮询 cancelFlag（软超时）
    try {
        result = dispatchExec(cfg, effectiveCancel, deadline);
    } catch (const std::exception& e) {
        result.status  = core::TaskStatus::Failed;
        result.message = std::string("TaskRunner: exception: ") + e.what();
    } catch (...) {
        result.status  = core::TaskStatus::Failed;
        result.message = "TaskRunner: unknown exception";
    }

    // 4) 执行返回后做一次收尾检查：如果外部已经取消且策略未显式标记，则覆盖为 Canceled
    if (effectiveCancel &&
        effectiveCancel->load(std::memory_order_acquire) &&
        result.status == core::TaskStatus::Success) {
        result.status  = core::TaskStatus::Canceled;
        result.message = "TaskRunner: canceled";
    }

    return result;
}

TaskResult TaskRunner::dispatchExec(const TaskConfig &cfg, std::atomic_bool *cancelFlag, SteadyClock::time_point deadline) const
{
    auto* strategy = ExecutionStrategyRegistry::instance().get(cfg.execType);
    if (!strategy) {
        core::TaskResult r;
        r.status  = core::TaskStatus::Failed;
        r.message = "没有为execType注册执行策略:"
                    + std::to_string(static_cast<int>(cfg.execType));
        return r;
    }

    // 真正执行一次（不含重试）
    return strategy->execute(cfg, cancelFlag, deadline);
}

// ... existing code ...

/// 执行本地任务
/// @param cfg 任务配置信息，包含任务ID或命令等执行所需参数
/// @param cancelFlag 取消标志，如果任务被取消则指向原子布尔值
/// @param deadline 任务截止时间点，超过该时间点任务应被终止
/// @return 返回任务执行结果，包括状态和相关信息
TaskResult TaskRunner::execLocal(const TaskConfig &cfg, std::atomic_bool *cancelFlag, SteadyClock::time_point deadline) const
{
    core::TaskResult r;

    // 确定任务键值，优先使用ID，若ID为空则使用执行命令作为键值
    const std::string key = !cfg.execCommand.empty()? cfg.execCommand : cfg.id.value;
    if (key.empty()) {
        r.status  = core::TaskStatus::Failed;
        r.message = "TaskRunner: empty local task id";
        return r;
    }

    // 在本地任务注册表中查找对应的任务函数
    auto func = LocalTaskRegistry::instance().find(key);
    if (!func) {
        r.status  = core::TaskStatus::Failed;
        r.message = "TaskRunner: local task not found: " + key;
        return r;
    }
    // 检查任务是否在执行前已被取消
    if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) {
        r.status  = core::TaskStatus::Canceled;
        r.message = "TaskRunner: canceled before local execution";
        return r;
    }

    // 执行任务函数并捕获可能的异常
    try {
        r = func(cfg, cancelFlag);
    } catch (const std::exception& ex) {
        r.status  = core::TaskStatus::Failed;
        r.message = std::string("TaskRunner: local task exception: ") + ex.what();
    } catch (...) {
        r.status  = core::TaskStatus::Failed;
        r.message = "TaskRunner: local task unknown exception";
    }
    //
    Logger::info("TaskRunner::execLocal, id=" + cfg.id.value +
                 ", name=" + cfg.name +
                 ", status=" + std::to_string(static_cast<int>(r.status)) +
                 ", message=" + r.message);
    return r;
}

// ... existing code ...

/// 执行Shell命令任务
/// @param cfg 任务配置信息，包含要执行的Shell命令等参数
/// @param cancelFlag 取消标志，用于检查任务是否被取消
/// @param deadline 任务截止时间点，用于超时控制
/// @return 返回任务执行结果，包括执行状态、消息和执行时长
TaskResult TaskRunner::execShell(const TaskConfig &cfg, std::atomic_bool *cancelFlag, SteadyClock::time_point deadline) const
{
    core::TaskResult r;

    const auto start = SteadyClock::now();

    // 检查执行命令是否为空
    if (cfg.execCommand.empty()) {
        r.status  = core::TaskStatus::Failed;
        r.message = "TaskRunner: empty shell command";
        return r;
    }

    // 检查任务是否在执行前已被取消
    if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) {
        r.status  = core::TaskStatus::Canceled;
        r.message = "TaskRunner: canceled before shell execution";
        return r;
    }

    // 执行Shell命令
    const int code = std::system(cfg.execCommand.c_str());

    // 根据执行结果和取消状态设置返回值
    if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) {
        r.status  = core::TaskStatus::Canceled;
        r.message = "TaskRunner: canceled during shell execution";
    } else if (code == 0) {
        r.status = core::TaskStatus::Success;
    } else {
        r.status  = core::TaskStatus::Failed;
        r.message = "TaskRunner: shell exit code " + std::to_string(code);
    }
    r.exitCode = code;
    const auto end = SteadyClock::now();
    r.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    Logger::info("TaskRunner::execShell, id=" + cfg.id.value +
                 ", name=" + cfg.name +
                 ", status=" + std::to_string(static_cast<int>(r.status)) +
                 ", message=" + r.message +
                 ", duration=" + std::to_string(r.durationMs) + "ms");
   
    return r;
}
/// 执行HTTP调用任务
/// @param cfg 任务配置信息，包含URL和其他执行参数
/// @param cancelFlag 取消标志，用于检查任务是否被取消
/// @param deadline 任务截止时间点，用于超时控制
/// @return 返回任务执行结果，包括执行状态、消息和执行时长
TaskResult TaskRunner::execHttpCall(const TaskConfig &cfg, std::atomic_bool *cancelFlag, SteadyClock::time_point deadline) const
{
    core::TaskResult r;

    const auto start = SteadyClock::now();

    if (cfg.execCommand.empty()) {
        r.status  = core::TaskStatus::Failed;
        r.message = "TaskRunner: empty http url";
        return r;
    }

    if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) {
        r.status  = core::TaskStatus::Canceled;
        r.message = "TaskRunner: canceled before http call";
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
        r.message = "TaskRunner: unsupported or invalid URL";
        return r;
    }

    httplib::Client cli(host, port);
    cli.set_keep_alive(false);

    if (deadline != SteadyClock::time_point::max()) {
        const auto now = SteadyClock::now();
        if (deadline <= now) {
            r.status  = core::TaskStatus::Timeout;
            r.message = "TaskRunner: timeout before http call";
            return r;
        }
        const auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
        const auto sec    = static_cast<int>(remain.count() / 1000);
        const auto usec   = static_cast<int>((remain.count() % 1000) * 1000);
        cli.set_connection_timeout(sec, usec);
        cli.set_read_timeout(sec, usec);
        cli.set_write_timeout(sec, usec);
    }

    httplib::Params params;
    for (const auto &kv : cfg.execParams) {
        params.emplace(kv.first, kv.second);
    }

    auto res = params.empty() ? cli.Get(path) : cli.Post(path, params);

    if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) {
        r.status  = core::TaskStatus::Canceled;
        r.message = "TaskRunner: canceled during http call";
    } else if (!res) {
        r.status  = core::TaskStatus::Failed;
        r.message = "TaskRunner: http error " + httplib::to_string(res.error());
    } else if (res->status >= 200 && res->status < 300) {
        r.status = core::TaskStatus::Success;
    } else {
        r.status  = core::TaskStatus::Failed;
        r.message = "TaskRunner: http status " + std::to_string(res->status);
    }
    const auto end = SteadyClock::now();

    r.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    Logger::info("TaskRunner::execHttpCall, id=" + cfg.id.value +
                 ", name=" + cfg.name +
                 ", status=" + std::to_string(static_cast<int>(r.status)) +
                 ", message=" + r.message);

    return r;
}

TaskResult TaskRunner::execScript(const TaskConfig &cfg, std::atomic_bool *cancelFlag, SteadyClock::time_point deadline) const
{
    return execShell(cfg, cancelFlag, deadline);
}

TaskResult TaskRunner::execRemote(const TaskConfig &cfg, std::atomic_bool *cancelFlag, SteadyClock::time_point deadline) const
{
    core::TaskResult r;

    // TODO（M11 使用）：向远程 Worker 发送任务：
    //   - 根据 cfg.metadata / queue / priority 选择 Worker
    //   - 通过 HTTP / RPC / 消息队列投递任务
    //   - 等待 Worker 回调 / 查询状态
    //   - 支持取消时，发 cancel 请求给 Worker
    //
    // 目前你可以先占位，返回 Failed 或 NotImplemented

    r.status  = core::TaskStatus::Failed;
    r.message = "TaskRunner::execRemote not implemented";
    return r;
}

} // namespace taskhub::runner
