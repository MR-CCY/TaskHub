//
// Created by 曹宸源 on 2025/11/11.
//
#include "logger.h"
#include <iostream>
#include <ctime>
#include <iomanip>
#include <sstream>
#include "log_manager.h"
#include "log_record.h"
#include "utils.h"

static std::string current_timestamp() {
    std::time_t t = std::time(nullptr);
    std::tm tm;

#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    std::stringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void taskhub::Logger::debug(const std::string &msg) {
    write(LogLevel::Debug, msg);
}

void taskhub::Logger::info(const std::string &msg) {
    write(LogLevel::Info, msg);
}

void taskhub::Logger::warn(const std::string &msg) {
    write(LogLevel::Warn, msg);
}

void taskhub::Logger::error(const std::string &msg) {
    write(LogLevel::Error, msg);
}

void taskhub::Logger::write(LogLevel level, const std::string &msg) {
    // Route legacy Logger messages into the M12 LogManager pipeline.
    // Use a dedicated system task id so it won't mix with real task logs.
    taskhub::core::LogRecord rec;
    rec.taskId.value = "_system";
    rec.stream = taskhub::core::LogStream::None;
    rec.message = msg;
    rec.ts = std::chrono::system_clock::now();
    // Map legacy LogLevel -> core::LogLevel
    switch (level) {
        case LogLevel::Debug: rec.level = LogLevel::Debug; break;
        case LogLevel::Info:  rec.level = LogLevel::Info;  break;
        case LogLevel::Warn:  rec.level = LogLevel::Warn;  break;
        case LogLevel::Error: rec.level = LogLevel::Error; break;
        default:              rec.level = LogLevel::Info;  break;
    }

    // Add a couple of helpful fields for system logs.
    rec.fields["source"] = "Logger";

    try {
        taskhub::core::LogManager::instance().emit(rec);
    } catch (...) {
        // Fallback (should be rare): keep old behavior so we never lose logs.
        std::string time = utils::now_string();
        std::string lvl  = level_to_string(level);
        std::cout << time << " [" << lvl << "] " << msg << std::endl;
    }
}

std::string taskhub::Logger::level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
        default: return "UNK";
    }
}
