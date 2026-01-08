#include "local_strategy.h"
#include "runner/local_task_registry.h"
#include <thread>
#include <atomic>
namespace  taskhub::runner {
    core::TaskResult LocalExecutionStrategy::execute(core::ExecutionContext& ctx)
    {
        core::TaskResult r;
        const auto& cfg = ctx.config;
        auto deadline = ctx.getDeadline();

        const std::string key = ctx.get("handler", !cfg.execCommand.empty()? cfg.execCommand : cfg.id.value);
        if (key.empty()) {
            return ctx.fail("TaskRunner：本地任务id为空");
        }

        auto func = LocalTaskRegistry::instance().find(key);
        if (!func) {
            return ctx.fail("TaskRunner：未找到本地任务: " + key);
        }

        if (ctx.isCanceled()) {
            return ctx.canceled("TaskRunner：在本地执行前取消");
        }

        std::atomic_bool timeoutFlag{false};
        std::atomic_bool doneFlag{false};
        std::thread watchdog;
        if (deadline != Deadline::max()) {
            watchdog = std::thread([&]() {
                while (!doneFlag.load(std::memory_order_acquire)) {
                    if (ctx.isCanceled()) {
                        return;
                    }
                    if (ctx.isTimeout()) {
                        timeoutFlag.store(true, std::memory_order_release);
                        // 触发协作取消
                        // Note: ExecutionContext uses the same cancelFlag passed from outside
                        return;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
            });
        }

        const auto start = SteadyClock::now();
        try {
            r = func(ctx);
            core::emitEvent(cfg.id, LogLevel::Info,"Local finished, status=" + std::to_string((int)r.status) +", durationMs=" + std::to_string(r.durationMs));
        } catch (const std::exception& ex) {
            r = ctx.fail(std::string("TaskRunner：本地任务异常: ") + ex.what());
        } catch (...) {
            r = ctx.fail("TaskRunner：本地任务未知异常");
        }

        doneFlag.store(true, std::memory_order_release);

        if (watchdog.joinable()) {
            watchdog.join();
        }

        if (timeoutFlag.load(std::memory_order_acquire)) {
            r = ctx.timeout("TaskRunner：本地任务执行超时（软超时）");
        }

        const auto end = SteadyClock::now();
        r.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        Logger::info("LocalExecutionStrategy::execute, id=" + cfg.id.value +
            ", name=" + cfg.name +
            ", status=" + std::to_string(static_cast<int>(r.status)) +
            ", durationMs=" + std::to_string(r.durationMs) +
            ", timeoutFlag=" + std::string(timeoutFlag.load(std::memory_order_acquire) ? "true" : "false") +
            ", message=" + r.message);
        return r;
    }
}
