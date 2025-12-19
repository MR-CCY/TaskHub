#pragma once
#include <unordered_map>
#include <deque>
#include <mutex>
#include <vector>
#include <cstdint>
#include <chrono>

#include "log_record.h"

namespace taskhub::core {

class TaskLogBuffer {
public:
    struct QueryResult {
        std::uint64_t nextFrom{0};
        std::vector<LogRecord> records;
    };

    struct TaskKey {
        std::string id;
        std::string runId;
        bool operator==(const TaskKey& other) const {
            return id == other.id && runId == other.runId;
        }
    };

    struct TaskKeyHash {
        std::size_t operator()(const TaskKey& k) const noexcept {
            return std::hash<std::string>()(k.id) ^ (std::hash<std::string>()(k.runId) << 1);
        }
    };

    // 每个 task 默认最多保留多少条 record（防止内存爆）
    explicit TaskLogBuffer(std::size_t perTaskMaxRecords = 2000);

    // 追加一条结构化日志
    LogRecord append(LogRecord rec);

    // 便捷：追加 stdout/stderr（会转成 LogRecord）
    LogRecord  appendStdout(const TaskId& taskId, const std::string& text);
    LogRecord  appendStderr(const TaskId& taskId, const std::string& text);

    // 分页拉取：fromSeq 起（包含），最多 limit 条
    QueryResult get(const TaskId& taskId, std::uint64_t fromSeq, std::size_t limit) const;

    // 尾部 N 条
    std::vector<LogRecord> tail(const TaskId& taskId, std::size_t n) const;

    // 清理某个 task 的日志
    void clear(const TaskId& taskId);

    // 清理很久不访问的 task（可选）
    void pruneOlderThan(std::chrono::milliseconds maxAge);

private:
    struct PerTaskBuf {
        std::deque<LogRecord> q;
        std::uint64_t nextSeq{1};
        std::chrono::steady_clock::time_point lastTouch{std::chrono::steady_clock::now()};
    };

    PerTaskBuf& getOrCreate_(const TaskId& taskId);
    PerTaskBuf* find_(const TaskId& taskId);
    const PerTaskBuf* find_(const TaskId& taskId) const;

private:
    std::size_t _perTaskMaxRecords;
    mutable std::mutex _mu;
    std::unordered_map<TaskKey, PerTaskBuf, TaskKeyHash> _bufs; // key = (taskId.value, runId)
};

} // namespace taskhub::core
