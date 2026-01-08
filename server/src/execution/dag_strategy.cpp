#include "dag_strategy.h"
#include "dag/dag_service.h"
#include "dag/dag_run_utils.h"
#include "log/logger.h"
#include "utils/utils.h"
#include "json.hpp"

namespace taskhub::runner {

using json = nlohmann::json;

namespace {
    bool parse_dag_body(const core::TaskConfig& cfg, json& out, std::string& err)
    {
        auto it = cfg.execParams.find("dag_json");
        std::string raw;
        if (it != cfg.execParams.end()) {
            raw = it->second;
        } else if (!cfg.execCommand.empty()) {
            raw = cfg.execCommand;
        }

        if (raw.empty()) {
            err = "DagExecution: missing dag_json";
            return false;
        }

        out = json::parse(raw, nullptr, false);
        if (out.is_discarded() || !out.is_object()) {
            err = "DagExecution: invalid dag_json";
            return false;
        }
        return true;
    }
}

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

    json dagBody;
    std::string err;
    if (!parse_dag_body(cfg, dagBody, err)) {
        return ctx.fail(err);
    }

    if (!dagBody.contains("config") || !dagBody["config"].is_object()) {
        dagBody["config"] = json::object();
    }
    if (!cfg.id.runId.empty()) {
        dagBody["config"]["parent_run_id"] = cfg.id.runId;
    }
    if (!cfg.id.value.empty()) {
        dagBody["config"]["parent_task_id"] = cfg.id.value;
    }
    if (!dagBody.contains("name") || !dagBody["name"].is_string()) {
        const std::string fallback = cfg.name.empty() ? cfg.id.value : cfg.name;
        if (!fallback.empty()) {
            dagBody["name"] = fallback;
        }
    }
    if (!dagBody["config"].contains("name")) {
        dagBody["config"]["name"] = dagBody.value("name", std::string{});
    }

    const std::string runId = std::to_string(utils::now_millis()) + "_" + utils::random_string(6);
    dagrun::injectRunId(dagBody, runId);
    dagrun::persistRunAndTasks(runId, dagBody, "task_dag");
    result.metadata["run_id"] = runId;

    const auto start = SteadyClock::now();
    auto dagResult = dag::DagService::instance().runDag(dagBody, runId);
    const auto end = SteadyClock::now();
    result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    result.status = dagResult.success ? core::TaskStatus::Success : core::TaskStatus::Failed;
    result.message = dagResult.message;
    return result;
}

} // namespace taskhub::runner
