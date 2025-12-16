//
// Created by 曹宸源 on 2025/11/11.
//
#pragma once
#include <string>

namespace taskhub {

enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error
};

class Logger {
public:
    static void debug(const std::string& msg);
    static void info(const std::string& msg);
    static void warn(const std::string& msg);
    static void error(const std::string& msg);

private:
    static void write(LogLevel level, const std::string& msg);
    static std::string level_to_string(LogLevel level);
};

} // namespace taskhub