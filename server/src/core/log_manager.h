#pragma once
#include <memory>
#include <vector>
#include <mutex>

#include "log_record.h"
#include "task_log_buffer.h"
#include "runner/task_config.h"
namespace taskhub::core {

// M12.1：先只做 buffer + emit
// 后续 M12.2：再加 sinks（console/file/rotate）
class LogManager {
public:
    static LogManager& instance();

    void init(std::size_t perTaskMaxRecords = 2000);

    // 统一入口：写入 TaskLogBuffer（后续再广播 sinks/ws）
    void emit(const LogRecord& rec);

    // 便捷：写 stdout/stderr
    void stdoutLine(const TaskId& taskId, const std::string& text);
    void stderrLine(const TaskId& taskId, const std::string& text);

    // 查询
    TaskLogBuffer::QueryResult query(const TaskId& taskId, std::uint64_t fromSeq, std::size_t limit) const;
    std::vector<LogRecord> tail(const TaskId& taskId, std::size_t n) const;

    // 清理
    void clear(const TaskId& taskId);
    void pruneOlderThan(std::chrono::milliseconds maxAge);

private:
    LogManager() = default;

private:
    mutable std::mutex _mu;
    std::unique_ptr<TaskLogBuffer> _buffer;
};
void emitEvent(const TaskId& taskId, LogLevel level, const std::string& msg);
void emitEvent(const TaskConfig& cfg,
                LogLevel level,
                const std::string& msg,
                long long durationMs,
                int attempt = 1,
                const std::unordered_map<std::string, std::string>& extra = {});
} // namespace taskhub::core
