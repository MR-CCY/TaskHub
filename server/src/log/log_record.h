#pragma once
#include <string>
#include <unordered_map>
#include <chrono>
#include <cstdint>
#include "runner/task_common.h"
#include "log/logger.h" 

namespace taskhub::core {

enum class LogStream : int {
    None   = 0,
    Stdout = 1,
    Stderr = 2,
    Event  = 3, // 状态/事件类日志（start/end/retry/timeout 等）
};

struct LogRecord {
    // ---- identity / routing ----
    TaskId taskId;                         // 必填：关联到哪个任务
    std::string dagRunId;                  // 可选：DAG run
    std::string cronJobId;                 // 可选：Cron job
    std::string workerId;                  // 可选：Worker

    // ---- content ----
    LogLevel level{LogLevel::Info};
    LogStream stream{LogStream::Event};
    std::string message;

    // ---- timing ----
    std::chrono::system_clock::time_point ts{std::chrono::system_clock::now()};
    std::int64_t durationMs{0};            // 可选：事件耗时/任务耗时
    int attempt{0};                        // 可选：第几次尝试（1..n）

    // ---- extra fields ----
    std::unordered_map<std::string, std::string> fields; // 任意扩展字段

    // 序列号（由缓冲区分配，用于分页/增量拉取）
    std::uint64_t seq{0};
};

// 小工具：把 time_point 转成毫秒时间戳（便于 JSON 输出）
inline std::int64_t toEpochMs(std::chrono::system_clock::time_point tp) {
    using namespace std::chrono;
    return duration_cast<milliseconds>(tp.time_since_epoch()).count();
}

} // namespace taskhub::core