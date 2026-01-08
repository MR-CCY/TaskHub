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
core::TaskResult taskAHandler(const core::ExecutionContext& ctx)
{
    Logger::info("[taskA] start="+utils::now_string());
    // TODO: 填你真实的逻辑，这里先 mock 一下
    Logger::info("taskAHandler running...");

    // 可以根据 ctx.config.execParams 做点事情
    Logger::info("[taskA] end="+utils::now_string());
    return ctx.success("taskA done");
}

// B 节点对应的本地处理函数
core::TaskResult taskBHandler(const core::ExecutionContext& ctx)
{
    Logger::info("[taskB] start="+utils::now_string());
    Logger::info("taskBHandler running...");
    Logger::info("[taskB] end="+utils::now_string());
    return ctx.success("taskB done");
}
core::TaskResult taskCHandler(const core::ExecutionContext& ctx)
{
    Logger::info("[taskC] start="+utils::now_string());
    Logger::info("taskCHandler running...");
    Logger::info("[taskC] end="+utils::now_string());
    return ctx.success("taskC done");
}
core::TaskResult taskDHandler(const core::ExecutionContext& ctx)
{
    Logger::info("[taskD] start="+utils::now_string());
    Logger::info("taskDHandler running...");
    Logger::info("[taskD] end="+utils::now_string());
    return ctx.success("taskD done");
}
core::TaskResult taskBFailHandler(const core::ExecutionContext& ctx)
{
    Logger::info("[taskBFailHandler] start="+utils::now_string());
    Logger::info("taskBFailHandler running...");
    Logger::info("[taskBFailHandler] end="+utils::now_string());
    return ctx.fail("taskBFailHandler failed");
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

        // 数学函数
        lfr.registerTask("math_add", [](const core::ExecutionContext& ctx) {
            double a = std::stod(ctx.get("a", "0"));
            double b = std::stod(ctx.get("b", "0"));
            return ctx.success(std::to_string(a + b));
        });
        lfr.registerTask("math_sub", [](const core::ExecutionContext& ctx) {
            double a = std::stod(ctx.get("a", "0"));
            double b = std::stod(ctx.get("b", "0"));
            return ctx.success(std::to_string(a - b));
        });
        lfr.registerTask("math_mul", [](const core::ExecutionContext& ctx) {
            double a = std::stod(ctx.get("a", "0"));
            double b = std::stod(ctx.get("b", "0"));
            return ctx.success(std::to_string(a * b));
        });
        lfr.registerTask("math_div", [](const core::ExecutionContext& ctx) {
            double a = std::stod(ctx.get("a", "0"));
            double b = std::stod(ctx.get("b", "1"));
            if (b == 0) return ctx.fail("Division by zero");
            return ctx.success(std::to_string(a / b));
        });
        lfr.registerTask("math_mod", [](const core::ExecutionContext& ctx) {
            long long a = std::stoll(ctx.get("a", "0"));
            long long b = std::stoll(ctx.get("b", "1"));
            if (b == 0) return ctx.fail("Modulo by zero");
            return ctx.success(std::to_string(a % b));
        });
        lfr.registerTask("math_cmp", [](const core::ExecutionContext& ctx) {
            double a = std::stod(ctx.get("a", "0"));
            double b = std::stod(ctx.get("b", "0"));
            if (a > b) return ctx.success("gt");
            if (a < b) return ctx.success("lt");
            return ctx.success("eq");
        });
    }
};

static AutoRegisterLocalTasks s_autoRegisterLocalTasks;

} // namespace
