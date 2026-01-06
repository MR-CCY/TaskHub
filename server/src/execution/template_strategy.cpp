#include "template_strategy.h"
#include "template/template_store.h"
#include "template/template_renderer.h"
#include "dag/dag_service.h"
#include "dag/dag_run_utils.h"
#include "log/logger.h"
#include "utils/utils.h"
#include "json.hpp"

namespace taskhub::runner {

using json = nlohmann::json;

namespace {
    bool parse_template_params(const std::string& raw, json& out, std::string& err)
    {
        if (raw.empty()) {
            out = json::object();
            return true;
        }
        out = json::parse(raw, nullptr, false);
        if (out.is_discarded() || !out.is_object()) {
            err = "TemplateExecution: invalid template_params_json";
            return false;
        }
        return true;
    }
}

core::TaskResult TemplateExecutionStrategy::execute(const core::TaskConfig& cfg,
                                                    std::atomic_bool* cancelFlag,
                                                    Deadline deadline)
{
    core::TaskResult result;
    if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) {
        result.status = core::TaskStatus::Canceled;
        result.message = "TemplateExecution: canceled before start";
        return result;
    }
    if (deadline != Deadline::max() && SteadyClock::now() >= deadline) {
        result.status = core::TaskStatus::Timeout;
        result.message = "TemplateExecution: timeout before start";
        return result;
    }

    auto itId = cfg.execParams.find("template_id");
    if (itId == cfg.execParams.end() || itId->second.empty()) {
        result.status = core::TaskStatus::Failed;
        result.message = "TemplateExecution: missing template_id";
        return result;
    }

    json params;
    std::string err;
    auto itParams = cfg.execParams.find("template_params_json");
    if (!parse_template_params(itParams == cfg.execParams.end() ? "" : itParams->second, params, err)) {
        result.status = core::TaskStatus::Failed;
        result.message = err;
        return result;
    }

    auto tplOpt = tpl::TemplateStore::instance().get(itId->second);
    if (!tplOpt) {
        result.status = core::TaskStatus::Failed;
        result.message = "TemplateExecution: template not found";
        return result;
    }

    tpl::ParamMap p;
    for (auto& [k, v] : params.items()) {
        p[k] = v;
    }
    auto render = tpl::TemplateRenderer::render(*tplOpt, p);
    if (!render.ok) {
        result.status = core::TaskStatus::Failed;
        result.message = render.error;
        return result;
    }

    json rendered = std::move(render.rendered);
    if (!rendered.contains("config") || !rendered["config"].is_object()) {
        rendered["config"] = json::object();
    }
    rendered["config"]["template_id"] = itId->second;
    if (!cfg.id.runId.empty()) {
        rendered["config"]["parent_run_id"] = cfg.id.runId;
    }
    if (!cfg.id.value.empty()) {
        rendered["config"]["parent_task_id"] = cfg.id.value;
    }
    if (!rendered.contains("name")) {
        rendered["name"] = tplOpt->name;
    }
    if (!rendered["config"].contains("name")) {
        rendered["config"]["name"] = rendered.value("name", tplOpt->name);
    }

    const std::string runId = std::to_string(utils::now_millis()) + "_" + utils::random_string(6);
    dagrun::injectRunId(rendered, runId);
    dagrun::persistRunAndTasks(runId, rendered, "task_template");
    result.metadata["run_id"] = runId;
    result.metadata["template_id"] = itId->second;

    const auto start = SteadyClock::now();
    auto dagResult = dag::DagService::instance().runDag(rendered, runId);
    const auto end = SteadyClock::now();
    result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    result.status = dagResult.success ? core::TaskStatus::Success : core::TaskStatus::Failed;
    result.message = dagResult.message;
    return result;
}

} // namespace taskhub::runner
