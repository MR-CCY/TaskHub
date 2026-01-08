#include "script_strategy.h"
#include "execution_registry.h"
#include "runner/task_config.h"
namespace taskhub::runner {
    core::TaskResult ScriptExecutionStrategy::execute(core::ExecutionContext& ctx)
    {
        auto* strategy = ExecutionStrategyRegistry::instance().get(core::TaskExecType::Shell);
        if (!strategy) {
            return ctx.fail("没有为execType为Shell注册执行策略");
        }
    
        // 真正执行一次（不含重试）
        return strategy->execute(ctx);
    }
}