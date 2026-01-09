#pragma once
#include "execution_strategy.h"
namespace taskhub::worker{
    class WorkerInfo;
}
namespace taskhub::runner {
/// 本地执行策略：
/// - 对应 cfg.execType == Remote

class RemoteExecutionStrategy : public IExecutionStrategy {
public:
struct DispatchAttemptResult  {
    taskhub::core::TaskResult result;
    bool retryable{false};
    std::string retryReason;
};

    core::TaskResult execute(core::ExecutionContext& ctx) override;
private:
    DispatchAttemptResult dispatch_once(const worker::WorkerInfo& workerInfo,const core::ExecutionContext& ctx, const std::string& queue, const json& payloadJson);
};

} // namespace taskhub::runner
