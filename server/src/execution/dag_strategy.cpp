#include "dag_strategy.h"
#include "dag/dag_service.h"
#include "dag/dag_run_utils.h"
#include "log/logger.h"
#include "utils/utils.h"
#include "json.hpp"

namespace taskhub::runner {

using json = nlohmann::json;

core::TaskResult DagExecutionStrategy::execute(core::ExecutionContext& ctx)
{
    core::TaskResult result;
    const auto& cfg = ctx.config;

    if (ctx.isCanceled()) {
        result.status = core::TaskStatus::Canceled;
        result.message = "DagExecution: canceled before start";
        return result;
    }
    if (ctx.isTimeout()) {
        result.status = core::TaskStatus::Timeout;
        result.message = "DagExecution: timeout before start";
        return result;
    }

    const std::string runId = std::to_string(utils::now_millis()) + "_" + utils::random_string(6);
    result.metadata["run_id"] = runId;

    const auto start = SteadyClock::now();
    auto dagResult = dag::DagService::instance().runDag(cfg, runId);
    const auto end = SteadyClock::now();
    result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    result.status = dagResult.success ? core::TaskStatus::Success : core::TaskStatus::Failed;
    result.message = dagResult.message;
    return result;
}

} // namespace taskhub::runner
