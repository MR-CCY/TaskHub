#pragma once

#include <memory>
#include <unordered_map>
#include "runner/task_common.h"          // 里面应该有 TaskExecType
#include "execution_strategy.h"

namespace taskhub::runner {

/// 策略注册表：
/// - 按 TaskExecType 保存一个 IExecutionStrategy 实例
/// - TaskRunner::dispatchExec 通过它找到对应策略
class ExecutionStrategyRegistry {
public:
    static ExecutionStrategyRegistry& instance();

    void registerStrategy(core::TaskExecType type,
                          std::unique_ptr<IExecutionStrategy> strategy);

    /// 如果没注册，返回 nullptr
    IExecutionStrategy* get(core::TaskExecType type) const;

private:
    ExecutionStrategyRegistry() = default;

    std::unordered_map<core::TaskExecType, std::unique_ptr<IExecutionStrategy>> _map;
};

/// 注册内置策略（Local / Shell / Http / Script / Remote）
void registerDefaultExecutionStrategies();

} // namespace taskhub::runner