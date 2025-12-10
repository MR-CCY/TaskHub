#include "remote_strategy.h"
namespace taskhub::runner {
    core::TaskResult RemoteExecutionStrategy::execute(const core::TaskConfig &cfg, std::atomic_bool *cancelFlag, Deadline deadline)
    {
        core::TaskResult r;

        // TODO（M11 使用）：向远程 Worker 发送任务：
        //   - 根据 cfg.metadata / queue / priority 选择 Worker
        //   - 通过 HTTP / RPC / 消息队列投递任务
        //   - 等待 Worker 回调 / 查询状态
        //   - 支持取消时，发 cancel 请求给 Worker
        //
        // 目前你可以先占位，返回 Failed 或 NotImplemented
    
        r.status  = core::TaskStatus::Failed;
        r.message = "TaskRunner:：execRemote未实现";
        return r;
    }
}