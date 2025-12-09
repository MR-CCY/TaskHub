#include "taskRunner.h"
#include "core/logger.h"
#include <thread>
#include <future>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <exception>
#include <cstdlib>
#include "httplib.h"

namespace {

    using LocalTaskFn = taskhub::runner::LocalTaskFn;

    class LocalTaskRegistry {
    public:
        static LocalTaskRegistry& instance() {
            static LocalTaskRegistry inst;
            return inst;
        }

        void registerTask(const std::string& key, LocalTaskFn fn) {
            std::lock_guard<std::mutex> lock(mu_);
            registry_[key] = std::move(fn);
        }

        LocalTaskFn find(const std::string& key) const {
            std::lock_guard<std::mutex> lock(mu_);
            auto it = registry_.find(key);
            if (it == registry_.end()) {
                return nullptr;
            }
            return it->second;
        }

    private:
        LocalTaskRegistry() = default;
        LocalTaskRegistry(const LocalTaskRegistry&) = delete;
        LocalTaskRegistry& operator=(const LocalTaskRegistry&) = delete;

        mutable std::mutex mu_;
        std::unordered_map<std::string, LocalTaskFn> registry_;
    };

} // namespace

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
    return runWithRetry(cfg, cancelFlag);
}

void TaskRunner::registerLocalTask(const std::string &key, LocalTaskFn fn)
{
    LocalTaskRegistry::instance().registerTask(key, std::move(fn));
}

TaskResult TaskRunner::runWithRetry(const TaskConfig &cfg, std::atomic_bool *externalCancelFlag) const
{
    TaskResult lastResult;
    // retry次数
    int maxAttempts = cfg.retryCount + 1;
    auto baseDelay = cfg.retryDelay;
    if (baseDelay.count() <= 0) {
        baseDelay = std::chrono::milliseconds(1000);
    }
    for (int attempt = 0; attempt < maxAttempts; ++attempt) {

        // TODO Step 2：提前检查取消
        //   如果 externalCancelFlag && externalCancelFlag->load()，则直接返回 Canceled
        if (externalCancelFlag && externalCancelFlag->load(std::memory_order_acquire)) {
            lastResult.status  = core::TaskStatus::Canceled;
            lastResult.message = "TaskRunner: canceled before attempt";
            return lastResult;
        }

        // TODO Step 3：计算本次执行的 deadline
        SteadyClock::time_point deadline = SteadyClock::time_point::max();
        if (cfg.hasTimeout()) {
            deadline = SteadyClock::now() + cfg.timeout;
        }

        // TODO Step 4：真正执行一次
        lastResult = runOneAttempt(cfg, externalCancelFlag, deadline);

        // 成功就直接返回
        if (lastResult.ok()) {
            return lastResult;
        }

        // 如果是被取消 or 超时，可根据你的产品策略决定是否继续重试：
        // 常见做法：被取消 / 超时就不再重试
        if (lastResult.status == core::TaskStatus::Canceled ||
            lastResult.status == core::TaskStatus::Timeout) {
            break;
        }

        // 如果还没到最后一次尝试，则等待一段时间（重试间隔）
        if (attempt + 1 < maxAttempts) {
            auto delay = baseDelay;
            if (cfg.retryUseExponentialBackoff) {
                delay *= (1 << attempt); // 简单指数退避：1x,2x,4x...
            }

            // TODO Step 5：重试前的中断检查
            //   等待 delay 期间可以循环检测 cancelFlag，以便外部取消时不再重试
            auto sleepUntil = SteadyClock::now() + delay;
            while (SteadyClock::now() < sleepUntil) {
                if (externalCancelFlag && externalCancelFlag->load(std::memory_order_acquire)) {
                    lastResult.status  = core::TaskStatus::Canceled;
                    lastResult.message = "TaskRunner: canceled during retry backoff";
                    return lastResult;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
    }

    // TODO：这里可以打失败日志
    // LOG_WARN("TaskRunner::run failed after {} attempts, id={}", maxAttempts, cfg.id.value);
    return lastResult;
}

TaskResult TaskRunner::runOneAttempt(const TaskConfig &cfg, std::atomic_bool *externalCancelFlag, SteadyClock::time_point deadline) const
{
    TaskResult result;
      // TODO Step 1：为本次执行创建一个“内部取消标记”：
    //   - 如果 cfg.cancelable=false，则可以忽略外部 cancelFlag，或者只读不取消
    //   - 结合 externalCancelFlag，一起传给真正执行逻辑
    std::atomic_bool internalCancel{false};
    std::atomic_bool *effectiveCancel = &internalCancel;

    if (cfg.cancelable && externalCancelFlag) {
        effectiveCancel = externalCancelFlag;
    }
    Logger::info("TaskRunner::runOneAttempt, id=" + cfg.id.value +
                 ", name=" + cfg.name +
                 ", execType=" + std::to_string(static_cast<int>(cfg.execType)));
   // TODO Step 2：通过 async/thread 方式执行，以便 wait_for 实现超时
    auto future = std::async(std::launch::async, [this, &cfg, effectiveCancel, deadline]() {
        // 真正执行任务（不关心超时，只关心 cancelFlag）
        return dispatchExec(cfg, effectiveCancel, deadline);
    });       

    if (cfg.hasTimeout()) {
        // TODO Step 3：wait_for 等待结果直到 timeout
        auto now = SteadyClock::now();
        if (deadline <= now) {
            // timeout 已经过期
            if (effectiveCancel) {
                // 设置取消标记
                effectiveCancel->store(true, std::memory_order_release);
            }
            result.status  = core::TaskStatus::Timeout;
            result.message = "TaskRunner: timeout before execution";
            return result;
        }

        auto remain = deadline - now;
        if (future.wait_for(remain) == std::future_status::ready) {
            result = future.get();
        } else {
            // 超时：通知取消，并设置超时状态
            if (effectiveCancel) {
                effectiveCancel->store(true, std::memory_order_release);
            }
            result.status  = core::TaskStatus::Timeout;
            result.message = "TaskRunner: timeout during execution";
        }
    } else {
        // 没有超时，直接等待
        result = future.get();
    }

    return result;

}

TaskResult TaskRunner::dispatchExec(const TaskConfig &cfg, std::atomic_bool *cancelFlag, SteadyClock::time_point deadline) const
{
    // 根据 execType 分发执行逻辑
    Logger::info("TaskRunner::dispatchExec, id=" + cfg.id.value +
                 ", name=" + cfg.name +
                 ", execType=" + std::to_string(static_cast<int>(cfg.execType)));
    switch (cfg.execType) {
        case core::TaskExecType::Local:
            return execLocal(cfg, cancelFlag, deadline);
        case core::TaskExecType::Shell:
            return execShell(cfg, cancelFlag, deadline);
        case core::TaskExecType::HttpCall:
            return execHttpCall(cfg, cancelFlag, deadline);
        case core::TaskExecType::Script:
            return execScript(cfg, cancelFlag, deadline);
        case core::TaskExecType::Remote:
            return execRemote(cfg, cancelFlag, deadline);
        default: {
            core::TaskResult r;
            r.status  = core::TaskStatus::Failed;
            r.message = "TaskRunner: unknown execType";
            return r;
        }
    }
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
    //
    Logger::info("TaskRunner::execShell, id=" + cfg.id.value +
                 ", name=" + cfg.name +
                 ", status=" + std::to_string(static_cast<int>(r.status)) +
                 ", message=" + r.message +
                 ", duration=" + std::to_string(r.duration.count()));
    r.duration = std::chrono::duration_cast<std::chrono::milliseconds>(SteadyClock::now() - start);
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

    Logger::info("TaskRunner::execHttpCall, id=" + cfg.id.value +
                 ", name=" + cfg.name +
                 ", status=" + std::to_string(static_cast<int>(r.status)) +
                 ", message=" + r.message);
    r.duration = std::chrono::duration_cast<std::chrono::milliseconds>(SteadyClock::now() - start);
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
