#pragma once
#include <string>
#include <chrono>
#include <unordered_map>
#include "json.hpp"
using json = nlohmann::json;
#include "task_common.h"

namespace taskhub::core {

/// 任务优先级（用于 M9）
enum class TaskPriority : int {
    Low     = -1,
    Normal  = 0,
    High    = 1,
    Critical= 2,
};

/// 任务执行方式（未来扩展）
enum class TaskExecType {
    Local,      // 本地执行（默认）
    Remote,     // 走 Worker（M11）
    Script,     // 执行脚本
    HttpCall,   // 调用 HTTP API
    Shell,      // Shell 命令
    Dag,        // 同步执行 DAG
    Template,   // 同步执行模板
};

/// TaskConfig：描述一个任务的所有必要执行参数
struct TaskConfig {
    // ---- 基本信息 ----
    TaskId id;                  // 任务 ID（唯一）
    std::string name;           // 显示名字（可选）
    std::string description;    // 描述信息（可选）

    // ---- 执行方式（Hook 到不同 Runner）----
    TaskExecType execType{ TaskExecType::Local };
    std::string execCommand;    // 脚本 / Shell / HTTP URL / 指令（视 execType 而定）
    //Shell
    // { "cwd": "/tmp", "env.PATH": "...", "shell": "/bin/bash" }
    // HttpCall
    // { "method": "POST", "header.Authorization": "...", "body": "..." }
    // Script
    // { "interpreter": "python3", "args": "-u main.py" }
    // Local
    // { "handler": "taskA_handler" }
    // Remote
    // { "inner_exec_type": "Shell" }
    // Dag
    // { "dag_json": "{...}" }
    // Template
    // { "template_id": "tpl-1", "template_params_json": "{...}" }
    std::unordered_map<std::string, std::string> execParams; // 参数包

    // ---- 超时配置（M7）----
    std::chrono::milliseconds timeout{ std::chrono::milliseconds(0) };  
    /*  
        0 或 <=0 表示不超时  
        例如 5000ms 表示 5 秒强制中止  
    */

    // ---- 重试策略（M7）----
    int retryCount{ 0 };        // 重试次数
    std::chrono::milliseconds retryDelay{ std::chrono::milliseconds(1000) };   // 初次等待
    bool retryUseExponentialBackoff{ true }; // 指数退避（2x,4x,8x）

    // ---- 取消策略（M7）----
    bool cancelable{ true };    // 是否允许在执行中被取消

    // ---- 优先级（M9）----
    //todo:优先级还没做吧
    TaskPriority priority{ TaskPriority::Normal };

    // ---- 队列分组（M9）----
    std::string queue;          // 任务所属队列，比如 "default", "io", "cpu"

    // ---- 将来分布式执行时使用（M11） ----
    std::unordered_map<std::string, std::string> metadata; // Worker 标签选择、用户、租户、节点偏好等

    // ---- UI 显示、日志收集 ----
    bool captureOutput{ true }; // 是否捕获 Runner 输出（日志、stdout）

    // ---- 工具函数 ----
    bool hasTimeout() const {
        return timeout.count() > 0;
    }

};

inline std::string TaskExecTypetoString(TaskExecType type){
    switch (type) {
        case TaskExecType::Local:
            return "Local";
        case TaskExecType::Remote:
            return "Remote";
        case TaskExecType::Script:
            return "Script";
        case TaskExecType::HttpCall:
            return "HttpCall";
        case TaskExecType::Shell:
            return "Shell";
        case TaskExecType::Dag:
            return "Dag";
        case TaskExecType::Template:
            return "Template";
        default:
            return "Unknown";
    }
}

inline TaskExecType StringToTaskExecType(std::string type){
    std::transform(type.begin(), type.end(), type.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    if (type == "local")   return TaskExecType::Local;
    if (type == "remote")  return TaskExecType::Remote;
    if (type == "script")  return TaskExecType::Script;
    if (type == "httpcall" || type == "http_call" || type == "http") return TaskExecType::HttpCall;
    if (type == "shell")   return TaskExecType::Shell;
    if (type == "dag")     return TaskExecType::Dag;
    if (type == "template" || type == "temple") return TaskExecType::Template;

    return TaskExecType::Local;
}
inline int toPriorityInt(taskhub::core::TaskPriority p) {
    return static_cast<int>(p);
}

inline json buildRequestJson(const taskhub::core::TaskConfig& cfg)
{
    json jTask;

    jTask["id"]            = cfg.id.value;
    jTask["name"]          = cfg.name;
    jTask["exec_type"]     = TaskExecTypetoString(cfg.execType);
    jTask["exec_command"]  = cfg.execCommand;

    // exec_params
    json jParams = json::object();
    for (const auto& kv : cfg.execParams) {
        jParams[kv.first] = kv.second;
    }
    
    jTask["exec_params"] = jParams;

    // timeout/retry
    jTask["timeout_ms"]           = static_cast<long long>(cfg.timeout.count());
    jTask["retry_count"]          = cfg.retryCount;
    jTask["retry_delay_ms"]       = static_cast<long long>(cfg.retryDelay.count());
    jTask["retry_exp_backoff"]    = cfg.retryUseExponentialBackoff;

    // priority/queue/output
    jTask["priority"]       = toPriorityInt(cfg.priority);
    jTask["queue"]          = cfg.queue;
    jTask["capture_output"] = cfg.captureOutput;

    // metadata
    json jMeta = json::object();
    for (const auto& kv : cfg.metadata) {
        jMeta[kv.first] = kv.second;
    }
    jTask["metadata"] = jMeta;

    // 顶层封装（方便未来扩展）
    json jReq;
    jReq["task"] = jTask;
    return jReq;
}

inline TaskConfig parseTaskConfigFromReq(const json& jReq) {
      // ✅ 兼容两种格式：
    // 1) { "task": { ... } }
    // 2) { ... }  （直接就是 task）
    const json& jt = (jReq.contains("task") && jReq["task"].is_object())
                         ? jReq["task"]
                         : jReq;

    core::TaskConfig cfg;
    cfg.id.value        = jt.value("id", "");
    cfg.name            = jt.value("name", "");
    cfg.execType        = StringToTaskExecType(jt.value("exec_type", "Local"));
    cfg.execCommand     = jt.value("exec_command", "");
    cfg.queue           = jt.value("queue", "");
    cfg.captureOutput   = jt.value("capture_output", true);

    // timeout/retry
    cfg.timeout   = std::chrono::milliseconds(jt.value("timeout_ms", 0LL));
    cfg.retryCount = jt.value("retry_count", 0);
    cfg.retryDelay = std::chrono::milliseconds(jt.value("retry_delay_ms", 1000LL));
    cfg.retryUseExponentialBackoff = jt.value("retry_exp_backoff", true);

    // priority（int）
    cfg.priority = static_cast<core::TaskPriority>(jt.value("priority", 0));
    //不允许task的等级高于Critical
    if(cfg.priority==core::TaskPriority::Critical){
        cfg.priority = core::TaskPriority::High;
    }
    // exec_params
    if (jt.contains("exec_params") && jt["exec_params"].is_object()) {
        for (auto it = jt["exec_params"].begin(); it != jt["exec_params"].end(); ++it) {
            if (it.value().is_string()) cfg.execParams[it.key()] = it.value().get<std::string>();
            else cfg.execParams[it.key()] = it.value().dump();
        }
    }

    // metadata
    if (jt.contains("metadata") && jt["metadata"].is_object()) {
        for (auto it = jt["metadata"].begin(); it != jt["metadata"].end(); ++it) {
            if (it.value().is_string()) cfg.metadata[it.key()] = it.value().get<std::string>();
            else cfg.metadata[it.key()] = it.value().dump();
        }
    }

    return cfg;
}
inline bool startsWith(const std::string& s, const std::string& prefix){
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}
inline TaskConfig makeInnerTask(const TaskConfig& cfg) {
    TaskConfig inner = cfg; // 继承 timeout/retry/priority/queue/captureOutput/metadata 等

    // 1) inner.exec_type
    auto itType = cfg.execParams.find("inner.exec_type");
    if (itType != cfg.execParams.end()) {
        inner.execType = StringToTaskExecType(itType->second); // 你这个函数已支持大小写/下划线
    } else {
        // 没传就给个默认（按你的产品策略）
        inner.execType = TaskExecType::Shell;
    }

    // 2) inner.exec_command
    auto itCmd = cfg.execParams.find("inner.exec_command");
    if (itCmd != cfg.execParams.end()) {
        inner.execCommand = itCmd->second;
    } else {
        // 兜底：如果用户把命令写在 cfg.execCommand，也能跑
        inner.execCommand = cfg.execCommand;
    }

    // 3) inner.exec_params.*
    inner.execParams.clear();
    for (const auto& kv : cfg.execParams) {
        const std::string prefix = "inner.exec_params.";
        if (startsWith(kv.first, prefix)) {
            inner.execParams.emplace(kv.first.substr(prefix.size()), kv.second);
        }
    }

    return inner;
}
} // namespace taskhub::core
