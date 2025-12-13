#include "local_strategy.h"
#include "runner/local_task_registry.h"
#include <thread>
#include <atomic>

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

        // === 软超时（Soft Timeout）===
        // Local 任务无法安全强杀线程，只能：
        // 1) 到 deadline 时将 cancelFlag 置为 true（提示任务协作退出）
        // 2) 任务返回后，如果确实超时，则覆盖结果为 Timeout
        std::atomic_bool timeoutFlag{false};
        std::thread watchdog;
        if (deadline != Deadline::max()) {
            watchdog = std::thread([&]() {
                while (SteadyClock::now() < deadline) {
                    if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) {
                        return; // 外部已取消
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
                timeoutFlag.store(true, std::memory_order_release);
                if (cancelFlag) {
                    cancelFlag->store(true, std::memory_order_release);
                }
            });
        }

        // 执行任务函数并捕获可能的异常
        const auto start = SteadyClock::now();
        try {
            r = func(cfg, cancelFlag);
        } catch (const std::exception& ex) {
            r.status  = core::TaskStatus::Failed;
            r.message = std::string("TaskRunner：本地任务异常: ") + ex.what();
        } catch (...) {
            r.status  = core::TaskStatus::Failed;
            r.message = "TaskRunner：本地任务未知异常";
        }

        // 等待 watchdog 结束（避免 thread 泄漏）
        if (watchdog.joinable()) {
            watchdog.join();
        }

        // 如果发生了超时（软超时），则统一覆盖为 Timeout
        // 注意：即使任务返回 Success，只要超时，就按 Timeout 处理
        if (timeoutFlag.load(std::memory_order_acquire)) {
            r.status  = core::TaskStatus::Timeout;
            if (r.message.empty()) {
                r.message = "TaskRunner：本地任务执行超时（软超时）";
            }
        }

        const auto end = SteadyClock::now();
        r.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        Logger::info("LocalExecutionStrategy::execute, id=" + cfg.id.value +
            ", name=" + cfg.name +
            ", status=" + std::to_string(static_cast<int>(r.status)) +
            ", message=" + r.message);
        return r;
    }
}
