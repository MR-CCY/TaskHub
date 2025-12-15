#include "task_log_buffer.h"

namespace taskhub::core {

TaskLogBuffer::TaskLogBuffer(std::size_t perTaskMaxRecords)
    : _perTaskMaxRecords(perTaskMaxRecords) {}

TaskLogBuffer::PerTaskBuf& TaskLogBuffer::getOrCreate_(const TaskId& taskId) {
    auto& b = _bufs[taskId.value];
    b.lastTouch = std::chrono::steady_clock::now();
    return b;
}

TaskLogBuffer::PerTaskBuf* TaskLogBuffer::find_(const TaskId& taskId) {
    auto it = _bufs.find(taskId.value);
    if (it == _bufs.end()) return nullptr;
    it->second.lastTouch = std::chrono::steady_clock::now();
    return &it->second;
}

const TaskLogBuffer::PerTaskBuf* TaskLogBuffer::find_(const TaskId& taskId) const {
    auto it = _bufs.find(taskId.value);
    if (it == _bufs.end()) return nullptr;
    return &it->second;
}

LogRecord TaskLogBuffer::append(const LogRecord& rec) {
    std::lock_guard<std::mutex> lk(_mu);
    auto& b = getOrCreate_(rec.taskId);

    LogRecord r = rec;
    r.seq = b.nextSeq++;
    b.q.push_back(r);      

    while (b.q.size() > _perTaskMaxRecords) {
        b.q.pop_front();
    }
    return r;                    
}

LogRecord TaskLogBuffer::appendStdout(const TaskId& taskId, const std::string& text) {
    LogRecord r;
    r.taskId   = taskId;
    r.level    = LogLevel::Info;
    r.stream   = LogStream::Stdout;
    r.message  = text;
    return append(r);   // ✅ 返回带 seq 的那条
}

LogRecord TaskLogBuffer::appendStderr(const TaskId& taskId, const std::string& text) {
    LogRecord r;
    r.taskId   = taskId;
    r.level    = LogLevel::Warn;      // 你要也可用 Error，看你定义
    r.stream   = LogStream::Stderr;
    r.message  = text;
    return append(r);   // ✅ 返回带 seq 的那条
}

TaskLogBuffer::QueryResult TaskLogBuffer::get(const TaskId& taskId, std::uint64_t fromSeq, std::size_t limit) const {
    QueryResult out;
    std::lock_guard<std::mutex> lk(_mu);

    const auto* b = find_(taskId);
    if (!b || b->q.empty()) {
        out.nextFrom = fromSeq;
        return out;
    }

    // 注意：deque 里可能因为裁剪丢了更早 seq，因此先找到第一个 >= fromSeq
    for (const auto& rec : b->q) {
        if (rec.seq >= fromSeq) {
            out.records.push_back(rec);
            if (out.records.size() >= limit) break;
        }
    }

    if (!out.records.empty()) {
        out.nextFrom = out.records.back().seq + 1;
    } else {
        // 如果 fromSeq 太新（超过现存最大 seq），nextFrom 直接保持
        out.nextFrom = fromSeq;
    }
    return out;
}

std::vector<LogRecord> TaskLogBuffer::tail(const TaskId& taskId, std::size_t n) const {
    std::vector<LogRecord> out;
    std::lock_guard<std::mutex> lk(_mu);

    const auto* b = find_(taskId);
    if (!b) return out;

    const std::size_t sz = b->q.size();
    const std::size_t start = (n >= sz) ? 0 : (sz - n);
    out.reserve(sz - start);

    for (std::size_t i = start; i < sz; ++i) {
        out.push_back(b->q[i]);
    }
    return out;
}

void TaskLogBuffer::clear(const TaskId& taskId) {
    std::lock_guard<std::mutex> lk(_mu);
    _bufs.erase(taskId.value);
}

void TaskLogBuffer::pruneOlderThan(std::chrono::milliseconds maxAge) {
    std::lock_guard<std::mutex> lk(_mu);
    const auto now = std::chrono::steady_clock::now();

    for (auto it = _bufs.begin(); it != _bufs.end(); ) {
        auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.lastTouch);
        if (age > maxAge) it = _bufs.erase(it);
        else ++it;
    }
}

} // namespace taskhub::core