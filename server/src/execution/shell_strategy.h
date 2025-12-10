#pragma once
#include "execution_strategy.h"

namespace taskhub::runner {
/// 本地执行策略：
/// - 对应 cfg.execType == Shell
class ShellExecutionStrategy : public IExecutionStrategy {
public:
    core::TaskResult execute(const core::TaskConfig& cfg,
                             std::atomic_bool* cancelFlag,
                             Deadline deadline) override;
};

} // namespace taskhub::runner