#pragma once

#include <string>

namespace taskhub {
namespace utils {

// 当前时间，格式：YYYY-MM-DD HH:MM:SS.mmm
std::string now_string();

// 当前时间戳（毫秒）
long long now_millis();

std::pair<int, std::string> run_command(const std::string& cmd);
} // namespace utils
} // namespace taskhub