#pragma once
#include "execution_strategy.h"

namespace taskhub::runner {
/// 本地执行策略：
/// - 对应 cfg.execType == Script
class ScriptExecutionStrategy : public IExecutionStrategy {
public:
    core::TaskResult execute(core::ExecutionContext& ctx) override;
};

} // namespace taskhub::runner