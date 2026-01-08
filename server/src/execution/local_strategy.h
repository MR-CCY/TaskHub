#pragma once

#include "execution_strategy.h"

namespace taskhub::runner {

/// 本地执行策略：
/// - 对应 cfg.execType == Local
/// - 原来 TaskRunner::execLocal(...) 的逻辑剪切到这里
class LocalExecutionStrategy : public IExecutionStrategy {
public:
    core::TaskResult execute(core::ExecutionContext& ctx) override;
};

} // namespace taskhub::runner