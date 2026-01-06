#include "execution_registry.h"
#include "local_strategy.h"
#include "shell_strategy.h"
#include "http_strategy.h"
#include "script_strategy.h"
#include "remote_strategy.h"
#include "dag_strategy.h"
#include "template_strategy.h"
#include "log/logger.h"
namespace taskhub::runner {
    ExecutionStrategyRegistry &ExecutionStrategyRegistry::instance()
    {
        // TODO: 在此处插入 return 语句
        static ExecutionStrategyRegistry inst;
        return inst;
    }

    void ExecutionStrategyRegistry::registerStrategy(core::TaskExecType type, std::unique_ptr<IExecutionStrategy> strategy)
    {
        _map[type] = std::move(strategy);
    }

    IExecutionStrategy *ExecutionStrategyRegistry::get(core::TaskExecType type) const
    {
        auto it = _map.find(type);
        if (it == _map.end()) {
            return nullptr;
        }
        return it->second.get();
    }
    void registerDefaultExecutionStrategies()
    {
        auto& reg = ExecutionStrategyRegistry::instance();

        // Local
        reg.registerStrategy(core::TaskExecType::Local,std::make_unique<LocalExecutionStrategy>());
    
        //
        reg.registerStrategy(core::TaskExecType::Shell,std::make_unique<ShellExecutionStrategy>());
        //
        reg.registerStrategy(core::TaskExecType::HttpCall,
                             std::make_unique<HttpExecutionStrategy>());
        //
        reg.registerStrategy(core::TaskExecType::Script,
                             std::make_unique<ScriptExecutionStrategy>());
        //
        reg.registerStrategy(core::TaskExecType::Remote,
                             std::make_unique<RemoteExecutionStrategy>());

        reg.registerStrategy(core::TaskExecType::Dag,
                             std::make_unique<DagExecutionStrategy>());

        reg.registerStrategy(core::TaskExecType::Template,
                             std::make_unique<TemplateExecutionStrategy>());
    
        Logger::info("ExecutionStrategyRegistry: default strategies registered");
    }
}
