#include "local_strategy.h"
#include "runner/local_task_registry.h"

namespace  taskhub::runner {
    core::TaskResult LocalExecutionStrategy::execute(const core::TaskConfig &cfg, std::atomic_bool *cancelFlag, Deadline deadline)
    {
        core::TaskResult r;

        // 确定任务键值，优先使用ID，若ID为空则使用执行命令作为键值
        //todo: 考虑使用任务ID作为键值
        //需要做个Id=>execCommand的映射表
        
        // const std::string key = !cfg.id.value.empty() ? cfg.id.value : cfg.execCommand;
        const std::string key = !cfg.execCommand.empty()? cfg.execCommand : cfg.id.value;
        if (key.empty()) {
            r.status  = core::TaskStatus::Failed;
            r.message = "TaskRunner：本地任务id为空";
            return r;
        }
         // 在本地任务注册表中查找对应的任务函数
        auto func = LocalTaskRegistry::instance().find(key);
        if (!func) {
            r.status  = core::TaskStatus::Failed;
            r.message = "TaskRunner：未找到本地任务: " + key;
            return r;
        }
        // 检查任务是否在执行前已被取消
        if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) {
            r.status  = core::TaskStatus::Canceled;
            r.message = "TaskRunner：在本地执行前取消";
            return r;
        }

        // 执行任务函数并捕获可能的异常
        try {
            r = func(cfg, cancelFlag);
        } catch (const std::exception& ex) {
            r.status  = core::TaskStatus::Failed;
            r.message = std::string("TaskRunner：本地任务异常: ") + ex.what();
        } catch (...) {
            r.status  = core::TaskStatus::Failed;
            r.message = "TaskRunner：本地任务未知异常";
        }
        
        Logger::info("LocalExecutionStrategy::execute, id=" + cfg.id.value +
            ", name=" + cfg.name +
            ", status=" + std::to_string(static_cast<int>(r.status)) +
            ", message=" + r.message);
        return r;
    }
}
