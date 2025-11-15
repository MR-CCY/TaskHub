//
// Created by 曹宸源 on 2025/11/11.
//
#include "logger.h"
#include <iostream>
#include <ctime>
#include <iomanip>
#include <sstream>


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

void taskhub::Logger::init(const std::string &log_path) {
    (void)log_path;
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
    std::string time = current_timestamp();
    std::string lvl  = level_to_string(level);

    std::cout << time << " [" << lvl << "] " << msg << std::endl;
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
