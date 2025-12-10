#pragma once
#include <atomic>
#include <chrono>
#include "task_config.h"
#include "task_result.h"
#include <string>
#include <functional>
#include <atomic>

namespace taskhub::runner {

using LocalTaskFn = std::function<core::TaskResult(const core::TaskConfig&, std::atomic_bool*)>;
using SteadyClock = std::chrono::steady_clock;

/// TaskRunner：负责执行“一个任务”，并处理：
/// - 超时（timeout）
/// - 重试（retry）
/// - 取消（cancel）
class TaskRunner {
public:
    TaskRunner() = default;
    static TaskRunner& instance();

    /// 执行一个任务。
    /// @param cfg        任务配置（包含超时、重试、优先级等）
    /// @param cancelFlag 外部取消标记，可为 nullptr；DAG 或上层可传进来
    ///
    /// 这是对外唯一入口：所有执行都走这里。
    core::TaskResult run(const core::TaskConfig& cfg,std::atomic_bool* cancelFlag = nullptr) const;
        // ★ 新增：注册本地任务
        //不再使用
    void registerLocalTask(const std::string& key, LocalTaskFn fn);
    // void register_builtin_local_tasks();
private:
    /// 带重试逻辑的执行封装：
    /// - 根据 cfg.retryCount 决定重试次数
    /// - 根据 cfg.retryDelay / retryUseExponentialBackoff 控制间隔
    core::TaskResult runWithRetry(const core::TaskConfig& cfg,
                                  std::atomic_bool* externalCancelFlag) const;

    /// 一次尝试（不包含重试），但包含：
    /// - 超时控制
    /// - 取消控制
    ///
    /// @param deadline 如果 cfg.hasTimeout()==true，则为 now+timeout；
    ///                 否则可以是 time_point::max()
    core::TaskResult runOneAttempt(const core::TaskConfig& cfg,
                                   std::atomic_bool* externalCancelFlag,
                                   SteadyClock::time_point deadline) const;

    /// 真正执行任务的地方，根据 cfg.execType 分发到不同实现：
    core::TaskResult dispatchExec(const core::TaskConfig& cfg,
                                  std::atomic_bool* cancelFlag,
                                  SteadyClock::time_point deadline) const;
    //下面这个方式在M9已不再使用，已改用dispatchExec
    // === 针对不同 execType 的具体执行方法 ===
    core::TaskResult execLocal(const core::TaskConfig& cfg,
                               std::atomic_bool* cancelFlag,
                               SteadyClock::time_point deadline) const;

    core::TaskResult execShell(const core::TaskConfig& cfg,
                               std::atomic_bool* cancelFlag,
                               SteadyClock::time_point deadline) const;

    core::TaskResult execHttpCall(const core::TaskConfig& cfg,
                                  std::atomic_bool* cancelFlag,
                                  SteadyClock::time_point deadline) const;

    core::TaskResult execScript(const core::TaskConfig& cfg,
                                std::atomic_bool* cancelFlag,
                                SteadyClock::time_point deadline) const;

    core::TaskResult execRemote(const core::TaskConfig& cfg,
                                std::atomic_bool* cancelFlag,
                                SteadyClock::time_point deadline) const;
};

} // namespace taskhub::runner