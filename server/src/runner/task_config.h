#pragma once
#include <string>
#include <chrono>
#include <unordered_map>

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

} // namespace taskhub::core