#include "runner/taskRunner.h"
#include "runner/task_config.h"
#include "runner/task_result.h"
#include "core/logger.h"
using namespace taskhub;

namespace {

// A 节点对应的本地处理函数
core::TaskResult taskAHandler(const core::TaskConfig& cfg, std::atomic_bool* cancelFlag)
{
    core::TaskResult r;
    // TODO: 填你真实的逻辑，这里先 mock 一下
    Logger::info("taskAHandler running...");

    // 可以根据 cfg.execParams 做点事情
    r.status  = core::TaskStatus::Success;
    r.message = "taskA done";
    return r;
}

// B 节点对应的本地处理函数
core::TaskResult taskBHandler(const core::TaskConfig& cfg, std::atomic_bool* cancelFlag)
{
    core::TaskResult r;
    Logger::info("taskBHandler running...");
    r.status  = core::TaskStatus::Success;
    r.message = "taskB done";
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
        auto& runner = runner::TaskRunner::instance();
        runner.registerLocalTask("taskA_handler", taskAHandler);
        runner.registerLocalTask("taskB_handler", taskBHandler);
    }
};

static AutoRegisterLocalTasks s_autoRegisterLocalTasks;

} // namespace
