#pragma once
#include <string>
#include <cstdint>
#include "log_record.h"   // 你自己的 LogRecord 定义头文件

namespace taskhub::core {

// 简单：先做“文本一行”
// 例：
// ts_ms=... level=INFO stream=STDOUT task_id=... seq=12 attempt=1 duration_ms=7 msg="hello" k1=v1 k2=v2
class LogFormatter {
public:
    static LogFormatter& instance();

    // 一行文本（推荐先用这个）
    std::string formatLine(const LogRecord& r) const;

    // 预留：如果后面要 JSON Lines
    // std::string formatJsonLine(const LogRecord& r) const;

private:
    LogFormatter() = default;

    static const char* levelName_(LogLevel lv);
    static const char* streamName_(LogStream s);

    static std::string escapeMsg_(const std::string& s);
};

} // namespace taskhub::core