#pragma once

#include <atomic>
#include <chrono>

#include "runner/task_config.h"
#include "runner/task_result.h"
#include "log/logger.h"
#include "log/log_manager.h"
namespace taskhub::runner {

using SteadyClock = std::chrono::steady_clock;
using Deadline    = SteadyClock::time_point;

/// 执行策略抽象接口：
/// - 只负责“一次执行”（不含重试）
/// - TaskRunner 负责超时 / 重试 / 取消，然后调用这里的 execute()
class IExecutionStrategy {
public:
    virtual ~IExecutionStrategy() = default;

    /// 执行一次任务
    /// @param cfg        任务配置（包含 execType、命令、参数等）
    /// @param cancelFlag 取消标记，可为 nullptr
    /// @param deadline   超时时间点；如果没有超时限制，可以传 time_point::max()
    virtual core::TaskResult execute(const core::TaskConfig& cfg,
                                     std::atomic_bool* cancelFlag,
                                     Deadline deadline) = 0;
};

} // namespace taskhub::runner