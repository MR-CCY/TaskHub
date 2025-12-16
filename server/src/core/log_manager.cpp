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
    LogRecord stored;
    std::vector<std::shared_ptr<ILogSink>> sinksSnapshot;

    {
        std::lock_guard<std::mutex> lk(_mu);
        if (!_buffer) _buffer = std::make_unique<TaskLogBuffer>(2000);

        // ✅ append 返回带 seq 的记录
        stored = _buffer->append(rec);

        // ✅ 拷贝 sinks（避免锁内做 IO）
        sinksSnapshot = _sinks;
    }

    // ✅ WS 推送（用带 seq 的 stored）
    ws::WsLogStreamer::instance().pushLog(stored);

    // ✅ sinks 消费
    for (auto& s : sinksSnapshot) {
        if (s) s->consume(stored);
    }
}

void LogManager::addSink(std::shared_ptr<ILogSink> sink)
{
    if (!sink) return;
    std::lock_guard<std::mutex> lk(_mu);
    _sinks.push_back(std::move(sink));
}

void LogManager::clearSinks()
{
    std::lock_guard<std::mutex> lk(_mu);
    _sinks.clear();
}

void LogManager::setSinks(std::vector<std::shared_ptr<ILogSink>> sinks)
{
    std::lock_guard<std::mutex> lk(_mu);
    _sinks = std::move(sinks);
}

void LogManager::stdoutLine(const TaskId& taskId, const std::string& text) {
    LogRecord stored;
    std::vector<std::shared_ptr<ILogSink>> sinksSnapshot;

    {
        std::lock_guard<std::mutex> lk(_mu);
        if (!_buffer) _buffer = std::make_unique<TaskLogBuffer>(2000);

        // ✅ appendStdout 返回带 seq 的记录（你也要把它内部改成返回 stored）
        stored = _buffer->appendStdout(taskId, text);
        sinksSnapshot = _sinks;
    }

    ws::WsLogStreamer::instance().pushLog(stored);
    for (auto& s : sinksSnapshot) if (s) s->consume(stored);
}

void LogManager::stderrLine(const TaskId& taskId, const std::string& text) {
    LogRecord stored;
    std::vector<std::shared_ptr<ILogSink>> sinksSnapshot;

    {
        std::lock_guard<std::mutex> lk(_mu);
        if (!_buffer) _buffer = std::make_unique<TaskLogBuffer>(2000);

        stored = _buffer->appendStderr(taskId, text);
        sinksSnapshot = _sinks;
    }

    ws::WsLogStreamer::instance().pushLog(stored);
    for (auto& s : sinksSnapshot) if (s) s->consume(stored);
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