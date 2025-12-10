#include "shell_strategy.h"
namespace taskhub::runner {
    core::TaskResult ShellExecutionStrategy::execute(const core::TaskConfig &cfg, std::atomic_bool *cancelFlag, Deadline deadline)
    {
        core::TaskResult r;

        const auto start = SteadyClock::now();
    
        // 检查执行命令是否为空
        if (cfg.execCommand.empty()) {
            r.status  = core::TaskStatus::Failed;
            r.message = "TaskRunner：空Shell命令";
            return r;
        }
         // 检查任务是否在执行前已被取消
        if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) {
            r.status  = core::TaskStatus::Canceled;
            r.message = "TaskRunner：在Shell执行前取消";
            return r;
        }
        
        try {
            const int code = std::system(cfg.execCommand.c_str());
             // 根据执行结果和取消状态设置返回值
            if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) {
                r.status  = core::TaskStatus::Canceled;
                r.message = "TaskRunner：在Shell执行期间取消";
            } else if (code == 0) {
                r.status = core::TaskStatus::Success;
            } else {
                r.status  = core::TaskStatus::Failed;
                r.message = "TaskRunner：Shell退出代码 " + std::to_string(code);
            }
        } catch (const std::exception& ex) {
            r.status  = core::TaskStatus::Failed;
            r.message = std::string("TaskRunner：Shell任务异常: ") + ex.what();
        } catch (...) {
            r.status  = core::TaskStatus::Failed;
            r.message = "TaskRunner：Shell任务未知异常";
        }
        // 执行Shell命令
       

       
        //
        r.duration = std::chrono::duration_cast<std::chrono::milliseconds>(SteadyClock::now() - start);
        Logger::info("ShellExecutionStrategy::execute, id=" + cfg.id.value +
                    ", name=" + cfg.name +
                    ", status=" + std::to_string(static_cast<int>(r.status)) +
                    ", message=" + r.message +
                    ", duration=" + std::to_string(r.duration.count()));

        return r;
    }
}