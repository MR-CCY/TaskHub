#include "dag_strategy.h"
#include "dag/dag_service.h"
#include "dag/dag_run_utils.h"
#include "log/logger.h"
#include "utils/utils.h"
#include "json.hpp"
#include "dag/dag_thread_pool.h"
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

    // 检查嵌套深度限制
    if (ctx.nestingDepth() >= core::ExecutionContext::MAX_NESTING_DEPTH) {
        result.status = core::TaskStatus::Failed;
        result.message = "DAG nesting depth exceeds limit (" + 
                        std::to_string(core::ExecutionContext::MAX_NESTING_DEPTH) + ")";
        Logger::error("DagExecutionStrategy::execute failed: " + result.message +
                     ", current depth=" + std::to_string(ctx.nestingDepth()));
        return result;
    }

    std::string runId = ctx.get("manual_run_id");
    if (runId.empty()) {
        // Fallback: generate if not valid
        runId = std::to_string(utils::now_millis()) + "_" + utils::random_string(6);
    }
    result.metadata["run_id"] = runId;

    // Start event (包含嵌套深度信息)
    core::emitEvent(cfg, LogLevel::Info, 
                   "DagExecution start: runId=" + runId + 
                   ", nestingDepth=" + std::to_string(ctx.nestingDepth()), 
                   0);

    const auto start = SteadyClock::now();
    auto dagResult = dag::DagService::instance().runDag(cfg, runId);
    const auto end = SteadyClock::now();
    result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    result.status = dagResult.success ? core::TaskStatus::Success : core::TaskStatus::Failed;
    result.message = dagResult.message;
    result.exitCode = dagResult.success ? 0 : 1;

    // End event
    {
        const int st = static_cast<int>(result.status);
        std::string endMsg = "DagExecution end: status=" + std::to_string(st) +
                             ", durationMs=" + std::to_string(result.durationMs);

        if (!result.message.empty()) {
            endMsg += ", message=" + result.message;
        }

        LogLevel lvl = LogLevel::Info;
        if (result.status == core::TaskStatus::Failed)   lvl = LogLevel::Error;
        if (result.status == core::TaskStatus::Timeout)  lvl = LogLevel::Warn;
        if (result.status == core::TaskStatus::Canceled) lvl = LogLevel::Warn;

        core::emitEvent(cfg,
                  lvl,
                  endMsg,
                  result.durationMs,
                  /*attempt*/1,
                  {
                      {"status", core::TaskStatusTypetoString(result.status)},
                      {"run_id", runId}
                  });
    }

    Logger::info("DagExecutionStrategy::execute, id=" + cfg.id.value +
                ", name=" + cfg.name +
                ", status=" + core::TaskStatusTypetoString(result.status) +
                ", message=" + result.message +
                ", duration=" + std::to_string(result.durationMs) + "ms");

    return result;
}

} // namespace taskhub::runner
