#pragma once

#include "json.hpp"
#include "db/dag_run_repo.h"
#include "db/task_run_repo.h"

namespace taskhub::dagrun {

using json = nlohmann::json;

/// 注入 run_id 到常见的 dag payload 结构里，方便下游和存档查看
inline void injectRunId(json& body, const std::string& runId) {
    if (body.contains("tasks") && body["tasks"].is_array()) {
        for (auto& jt : body["tasks"]) {
            jt["run_id"] = runId;
        }
    } else if (body.contains("task") && body["task"].is_object()) {
        body["task"]["run_id"] = runId;
    } else {
        body["run_id"] = runId;
    }
}

/// 统一插入 dag_run 和初始 task_run 记录
inline void persistRunAndTasks(const std::string& runId,
                               const json& body,
                               const std::string& source,
                               const std::string& workflowJson = "") {
    DagRunRepo::instance().insertRun(runId, body, source, workflowJson);

    auto upsertTask = [&](const json& jt) {
        TaskRunRepo::instance().upsertFromTaskJson(runId, jt);
    };

    if (body.contains("tasks") && body["tasks"].is_array()) {
        for (const auto& jt : body["tasks"]) {
            upsertTask(jt);
        }
    } else if (body.contains("task") && body["task"].is_object()) {
        upsertTask(body["task"]);
    } else if (body.contains("id")) {
        upsertTask(body);
    }
}

} // namespace taskhub::dagrun
