#include "script_strategy.h"
#include "execution_registry.h"
#include "runner/task_config.h"
namespace taskhub::runner {
    core::TaskResult ScriptExecutionStrategy::execute(const core::TaskConfig &cfg, std::atomic_bool *cancelFlag, Deadline deadline)
    {
        auto* strategy = ExecutionStrategyRegistry::instance().get(core::TaskExecType::Shell);
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
}