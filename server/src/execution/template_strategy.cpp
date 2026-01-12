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
            err = "TemplateExecution: invalid params";
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

    // 检查嵌套深度限制
    if (ctx.nestingDepth() >= core::ExecutionContext::MAX_NESTING_DEPTH) {
        result.status = core::TaskStatus::Failed;
        result.message = "TemplateExecution: nesting depth exceeds limit (" +
                         std::to_string(core::ExecutionContext::MAX_NESTING_DEPTH) + ")";
        Logger::error("TemplateExecutionStrategy::execute failed: " + result.message +
                      ", current depth=" + std::to_string(ctx.nestingDepth()));
        return result;
    }

    std::string templateId = ctx.get("template_id");
    if (templateId.empty()) {
        return ctx.fail("TemplateExecution: missing template_id");
    }

    json params;
    std::string err;
    // Prefer exec_params.params (object) and fallback to legacy template_params_json (json string)
    std::string paramsJson = ctx.get("params");
    if (paramsJson.empty()) {
        paramsJson = ctx.get("template_params_json");
    }
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
    // Propagate nesting depth so DagService can inject child depth = parent+1
    rendered["_nesting_depth"] = ctx.nestingDepth();
    if (!rendered.contains("name")) {
        rendered["name"] = tplOpt->name;
    }
    if (!rendered["config"].contains("name")) {
        rendered["config"]["name"] = rendered.value("name", tplOpt->name);
    }

    std::string runId = ctx.get("run_id");
    if (runId.empty()) {
        runId = std::to_string(utils::now_millis()) + "_" + utils::random_string(6);
    }
    // Keep run_id in payload for easier debug/storage; DagService will persist (no double persist).
    dagrun::injectRunId(rendered, runId);
    result.metadata["run_id"] = runId;
    result.metadata["template_id"] = templateId;

    // Start event
    core::emitEvent(cfg,
                    LogLevel::Info,
                    "TemplateExecution start: templateId=" + templateId + ", runId=" + runId +
                        ", nestingDepth=" + std::to_string(ctx.nestingDepth()),
                    0,
                    /*attempt*/1,
                    {
                        {"run_id", runId},
                        {"template_id", templateId}
                    });

    const auto start = SteadyClock::now();
    auto dagResult = dag::DagService::instance().runDag(rendered, "task_template", runId);
    const auto end = SteadyClock::now();
    result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    result.status = dagResult.success ? core::TaskStatus::Success : core::TaskStatus::Failed;
    result.message = dagResult.message;
    result.exitCode = dagResult.success ? 0 : 1;

    // End event
    {
        LogLevel lvl = LogLevel::Info;
        if (result.status == core::TaskStatus::Failed) lvl = LogLevel::Error;
        if (result.status == core::TaskStatus::Timeout) lvl = LogLevel::Warn;
        if (result.status == core::TaskStatus::Canceled) lvl = LogLevel::Warn;

        core::emitEvent(cfg,
                        lvl,
                        "TemplateExecution end: status=" + std::to_string(static_cast<int>(result.status)) +
                            ", durationMs=" + std::to_string(result.durationMs),
                        result.durationMs,
                        /*attempt*/1,
                        {
                            {"status", core::TaskStatusTypetoString(result.status)},
                            {"run_id", runId},
                            {"template_id", templateId}
                        });
    }
    return result;
}

} // namespace taskhub::runner
