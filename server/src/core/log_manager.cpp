#include "log_manager.h"
#include "ws_log_streamer.h"
namespace taskhub::core {

LogManager& LogManager::instance() {
    static LogManager g;
    return g;
}

void LogManager::init(std::size_t perTaskMaxRecords) {
    std::lock_guard<std::mutex> lk(_mu);
    if (!_buffer) _buffer = std::make_unique<TaskLogBuffer>(perTaskMaxRecords);
}

void LogManager::emit(const LogRecord& rec) {
    std::lock_guard<std::mutex> lk(_mu);
    if (!_buffer) _buffer = std::make_unique<TaskLogBuffer>(2000);
    auto r =_buffer->append(rec);
    ws::WsLogStreamer::instance().pushLog(r);
}

void LogManager::stdoutLine(const TaskId& taskId, const std::string& text) {
    std::lock_guard<std::mutex> lk(_mu);
    if (!_buffer) _buffer = std::make_unique<TaskLogBuffer>(2000);
    LogRecord rec =  _buffer->appendStdout(taskId, text);
    ws::WsLogStreamer::instance().pushLog(rec);

}

void LogManager::stderrLine(const TaskId& taskId, const std::string& text) {
    std::lock_guard<std::mutex> lk(_mu);
    if (!_buffer) _buffer = std::make_unique<TaskLogBuffer>(2000);
    LogRecord rec =  _buffer->appendStderr(taskId, text);
    ws::WsLogStreamer::instance().pushLog(rec);
}

TaskLogBuffer::QueryResult LogManager::query(const TaskId& taskId, std::uint64_t fromSeq, std::size_t limit) const {
    std::lock_guard<std::mutex> lk(_mu);
    if (!_buffer) return {};
    return _buffer->get(taskId, fromSeq, limit);
}

std::vector<LogRecord> LogManager::tail(const TaskId& taskId, std::size_t n) const {
    std::lock_guard<std::mutex> lk(_mu);
    if (!_buffer) return {};
    return _buffer->tail(taskId, n);
}

void LogManager::clear(const TaskId& taskId) {
    std::lock_guard<std::mutex> lk(_mu);
    if (_buffer) _buffer->clear(taskId);
}

void LogManager::pruneOlderThan(std::chrono::milliseconds maxAge) {
    std::lock_guard<std::mutex> lk(_mu);
    if (_buffer) _buffer->pruneOlderThan(maxAge);
}

void emitEvent(const TaskId &taskId, LogLevel level, const std::string &msg)
{
    LogRecord rec;
    rec.taskId  = taskId;
    rec.stream  = LogStream::Event;
    rec.level   = level;
    rec.message = msg;
    LogManager::instance().emit(rec);
}

void emitEvent(const TaskConfig &cfg, 
                LogLevel level, 
                const std::string &msg,
                long long durationMs,
                int attempt,
                const std::unordered_map<std::string, std::string> &extra)
{
    taskhub::core::LogRecord rec;
    rec.taskId = cfg.id;
    rec.level = level;
    rec.stream = taskhub::core::LogStream::Event;
    rec.message = msg;
    rec.durationMs = durationMs;
    rec.attempt = attempt;

    // Common fields
    rec.fields["exec_type"] = taskhub::core::TaskExecTypetoString(cfg.execType);
    rec.fields["exec_command"] = cfg.execCommand;
    rec.fields["queue"] = cfg.queue;
    rec.fields["capture_output"] = cfg.captureOutput ? "true" : "false";

    for (const auto& kv : extra) {
        rec.fields[kv.first] = kv.second;
    }

    taskhub::core::LogManager::instance().emit(rec);
}

} // namespace taskhub::core