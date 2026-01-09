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

core::TaskResult TemplateExecutionStrategy::execute(core::ExecutionContext& ctx)
{
    core::TaskResult result;
    const auto& cfg = ctx.config;

    if (ctx.isCanceled()) {
        result.status = core::TaskStatus::Canceled;
        result.message = "TemplateExecution: canceled before start";
        return result;
    }
    if (ctx.isTimeout()) {
        result.status = core::TaskStatus::Timeout;
        result.message = "TemplateExecution: timeout before start";
        return result;
    }

    std::string templateId = ctx.get("template_id");
    if (templateId.empty()) {
        return ctx.fail("TemplateExecution: missing template_id");
    }

    json params;
    std::string err;
    std::string paramsJson = ctx.get("template_params_json");
    if (!parse_template_params(paramsJson, params, err)) {
        return ctx.fail(err);
    }

    auto tplOpt = tpl::TemplateStore::instance().get(templateId);
    if (!tplOpt) {
        return ctx.fail("TemplateExecution: template not found");
    }

    tpl::ParamMap p;
    for (auto& [k, v] : params.items()) {
        p[k] = v;
    }
    auto render = tpl::TemplateRenderer::render(*tplOpt, p);
    if (!render.ok) {
        return ctx.fail(render.error);
    }

    json rendered = std::move(render.rendered);
    if (!rendered.contains("config") || !rendered["config"].is_object()) {
        rendered["config"] = json::object();
    }
    rendered["config"]["template_id"] = templateId;
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

    std::string runId = ctx.get("manual_run_id");
    if (runId.empty()) {
        runId = std::to_string(utils::now_millis()) + "_" + utils::random_string(6);
    }
    dagrun::injectRunId(rendered, runId);
    dagrun::persistRunAndTasks(runId, rendered, "task_template");
    result.metadata["run_id"] = runId;
    result.metadata["template_id"] = templateId;

    const auto start = SteadyClock::now();
    auto dagResult = dag::DagService::instance().runDag(rendered, runId);
    const auto end = SteadyClock::now();
    result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    result.status = dagResult.success ? core::TaskStatus::Success : core::TaskStatus::Failed;
    result.message = dagResult.message;
    return result;
}

} // namespace taskhub::runner
