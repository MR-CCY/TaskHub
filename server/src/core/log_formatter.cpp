#include "log_formatter.h"
#include <sstream>
#include "utils.h"
namespace taskhub::core {

LogFormatter& LogFormatter::instance() {
    static LogFormatter f;
    return f;
}

const char* LogFormatter::levelName_(LogLevel lv) {
    switch (lv) {
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info:  return "INFO";
    case LogLevel::Warn:  return "WARN";
    case LogLevel::Error: return "ERROR";
    default:              return "INFO";
    }
}

const char* LogFormatter::streamName_(LogStream s) {
    switch (s) {
    case LogStream::Stdout: return "STDOUT";
    case LogStream::Stderr: return "STDERR";
    case LogStream::Event:  return "EVENT";
    case LogStream::None:  return "None";
    default:                return "EVENT";
    }
}

std::string LogFormatter::escapeMsg_(const std::string& s) {
    // 目标：单行日志，最小转义就够用（先不做完整 JSON escape）
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c == '"')  out += "\\\"";
        else out += c;
    }
    return out;
}

std::string LogFormatter::formatLine(const LogRecord& r) const {
    std::ostringstream oss;

    oss << "ts_ms=[" << utils::formatTimestampMs(r.ts)<<']'
        << " level=[" << levelName_(r.level)<<"]"
        << " stream=" << streamName_(r.stream)
        << " task_id=" << r.taskId.value
        << " seq=" << static_cast<unsigned long long>(r.seq);

    if (!r.dagRunId.empty())  oss << " dag_run_id=" << r.dagRunId;
    if (!r.cronJobId.empty()) oss << " cron_job_id=" << r.cronJobId;
    if (!r.workerId.empty())  oss << " worker_id=" << r.workerId;

    if (r.attempt > 0)        oss << " attempt=" << r.attempt;
    if (r.durationMs > 0)     oss << " duration_ms=" << r.durationMs;

    oss << " msg=\"" << escapeMsg_(r.message) << "\"";

    // extra fields：追加为 k=v（空格分隔）
    for (const auto& kv : r.fields) {
        oss << " " << kv.first << "=" << escapeMsg_(kv.second);
    }

    return oss.str();
}

} // namespace taskhub::core