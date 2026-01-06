#pragma once

#include <string>
#include <chrono>
#include <optional>
#include "cron_parser.h"
#include "runner/task_config.h" 
#include "dag/dag_types.h"
#include "dag/dag_builder.h"
#include "dag/dag_run_context.h"
#include "json.hpp"
namespace taskhub::scheduler {

/// 未来会扩展：单任务 / DAG 等
enum class CronTargetType {
    Unknown = 0,
    SingleTask,
    Dag,
    Template,
};

/// 一个 CronJob 的基本信息
struct CronJob {
    std::string id;      // 全局唯一 ID（以后可以用 UUID 或自增）
    std::string name;    // 可读名称
    std::string spec;    // 原始 cron 表达式，比如 "*/5 * * * *"

    CronTargetType targetType{ CronTargetType::Unknown };

    // 解析后的表达式
    CronExpr cronExpr;

    // 下一次触发时间（基于 system_clock）
    std::chrono::system_clock::time_point nextTime;

    // 是否启用（后面可以支持暂停）
    bool enabled{ true };

    // ======== M10.3 新增 payload =========
    // 1) 单任务：直接给 TaskRunner 一个 TaskConfig 模板
    std::optional<core::TaskConfig> taskTemplate;
    //
    struct DagJobPayload { 
        std::vector<dag::DagTaskSpec> specs; 
        dag::DagConfig config; 
        dag::DagEventCallbacks callbacks;
    };
    std::optional<DagJobPayload> dagPayload;
    struct TemplateJobPayload {
        std::string templateId;
        nlohmann::json params;
    };
    std::optional<TemplateJobPayload> templatePayload;

    // 构造函数：从 spec 解析出 cronExpr，并且计算 nextTime
    CronJob(const std::string& jobId,
            const std::string& jobName,
            const std::string& cronSpec,
            CronTargetType type);
};

} // namespace taskhub::scheduler
