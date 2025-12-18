// #include "runner/taskRunner.h"
#include "runner/task_config.h"
#include "runner/task_result.h"
#include "log/logger.h"
#include "utils/utils.h"
#include "runner/local_task_registry.h"
using namespace taskhub;
using namespace taskhub::runner;

namespace {

// A 节点对应的本地处理函数
core::TaskResult taskAHandler(const core::TaskConfig& cfg, std::atomic_bool* cancelFlag)
{
    Logger::info("[taskA] start="+utils::now_string());
    core::TaskResult r;
    // TODO: 填你真实的逻辑，这里先 mock 一下
    Logger::info("taskAHandler running...");

    // 可以根据 cfg.execParams 做点事情
    r.status  = core::TaskStatus::Success;
    r.message = "taskA done";
    Logger::info("[taskA] end="+utils::now_string());
    return r;
}

// B 节点对应的本地处理函数
core::TaskResult taskBHandler(const core::TaskConfig& cfg, std::atomic_bool* cancelFlag)
{
    Logger::info("[taskB] start="+utils::now_string());
    core::TaskResult r;
    Logger::info("taskBHandler running...");
    r.status  = core::TaskStatus::Success;
    r.message = "taskB done";
    Logger::info("[taskB] end="+utils::now_string());
    return r;   
}
core::TaskResult taskCHandler(const core::TaskConfig& cfg, std::atomic_bool* cancelFlag)
{
    Logger::info("[taskB] start="+utils::now_string());
    core::TaskResult r;
    Logger::info("taskCHandler running...");
    r.status  = core::TaskStatus::Success;
    r.message = "taskC done";
    Logger::info("[taskB] end="+utils::now_string());
    return r;
}
core::TaskResult taskDHandler(const core::TaskConfig& cfg, std::atomic_bool* cancelFlag)
{
    Logger::info("[taskD] start="+utils::now_string());
    core::TaskResult r;
    Logger::info("taskDHandler running...");
    r.status  = core::TaskStatus::Success;
    r.message = "taskD done";
    Logger::info("[taskD] end="+utils::now_string());
    return r;
}
core::TaskResult taskBFailHandler(const core::TaskConfig& cfg, std::atomic_bool* cancelFlag)
{
    Logger::info("[taskBFailHandler] start="+utils::now_string());
    core::TaskResult r;
    Logger::info("taskBFailHandler running...");
    r.status  = core::TaskStatus::Failed;
    r.message = "taskBFailHandler done";
    Logger::info("[taskBFailHandler] end="+utils::now_string());
    return r;
}
//     // ★ 显式暴露一个函数，由 ServerApp 在 init_dag() 调用
// void register_builtin_local_tasks()
// {
//     auto& r = runner::TaskRunner::instance();
//     r.registerLocalTask("taskA_handler", taskAHandler);
//     r.registerLocalTask("taskB_handler", taskBHandler);
// }
    
// 自动注册器：程序启动时自动把本地任务注册进 TaskRunner
struct AutoRegisterLocalTasks {
    AutoRegisterLocalTasks() {
        auto& lfr= LocalTaskRegistry::instance();

        lfr.registerTask("taskA_handler", taskAHandler);
        lfr.registerTask("taskB_handler", taskBHandler);
        lfr.registerTask("taskC_handler", taskCHandler);
        lfr.registerTask("taskD_handler", taskDHandler);
        lfr.registerTask("taskB_fail_handler", taskBFailHandler);
    }
};

static AutoRegisterLocalTasks s_autoRegisterLocalTasks;

} // namespace
