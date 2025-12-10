#include "cron_parser.h"
#include <array>
#include <ctime>
#include <cstdint>
#include <stdexcept>
namespace taskhub::scheduler {
    CronExpr::CronExpr(const std::string &spec)
    {
        parse(spec);
    }
    std::chrono::system_clock::time_point CronExpr::next(const std::chrono::system_clock::time_point &now) const
    {
        // 创建查找表，用于快速检查时间组件是否匹配cron表达式
        std::array<uint8_t, 60> minute_lookup{};
        std::array<uint8_t, 24> hour_lookup{};
        std::array<uint8_t, 32> day_lookup{};
        std::array<uint8_t, 13> month_lookup{};
        std::array<uint8_t, 7> weekday_lookup{};

        // 初始化查找表，将已解析的cron字段值标记为有效
        for (int v : minutes_) {
            minute_lookup[static_cast<size_t>(v)] = 1;
        }
        for (int v : hours_) {
            hour_lookup[static_cast<size_t>(v)] = 1;
        }
        for (int v : days_) {
            day_lookup[static_cast<size_t>(v)] = 1;
        }
        for (int v : months_) {
            month_lookup[static_cast<size_t>(v)] = 1;
        }
        for (int v : weekdays_) {
            weekday_lookup[static_cast<size_t>(v)] = 1;
        }

        // 从下一分钟开始检查，确保不会返回当前时间点
        const auto base = std::chrono::time_point_cast<std::chrono::minutes>(now) + std::chrono::minutes(1);
        // 设置最大检查次数为一年的分钟数，防止无限循环
        const int max_checks = 365 * 24 * 60;

        // 遍历检查每个候选时间点，直到找到匹配cron表达式的时刻
        for (int i = 0; i < max_checks; ++i) {
            const auto candidate = base + std::chrono::minutes(i);
            const std::time_t tt = std::chrono::system_clock::to_time_t(candidate);
            std::tm tm{};
            #if defined(_WIN32)
                        localtime_s(&tm, &tt);
            #else
                        localtime_r(&tt, &tm);
            #endif

            // 检查月份是否匹配
            const int month = tm.tm_mon + 1;
            if (!month_lookup[static_cast<size_t>(month)]) {
                continue;
            }
            // 检查日期是否匹配
            const int day = tm.tm_mday;
            if (!day_lookup[static_cast<size_t>(day)]) {
                continue;
            }
            // 检查星期几是否匹配
            const int weekday = tm.tm_wday;
            if (!weekday_lookup[static_cast<size_t>(weekday)]) {
                continue;
            }
            // 检查小时是否匹配
            const int hour = tm.tm_hour;
            if (!hour_lookup[static_cast<size_t>(hour)]) {
                continue;
            }
            // 检查分钟是否匹配
            const int minute = tm.tm_min;
            if (!minute_lookup[static_cast<size_t>(minute)]) {
                continue;
            }
            // 找到匹配的时间点，直接返回
            return candidate;
        }

        throw std::runtime_error("未能在一年内找到下一次执行时间");
    }
    void CronExpr::parse(const std::string &spec)
    {
        std::array<std::string, 5> fields;
        size_t idx = 0;
        const size_t len = spec.size();
        size_t i = 0;

        // 提取cron表达式的各个字段，跳过前导和中间的空格
        while (i < len && idx < fields.size()) {
            // 跳过空格字符
            while (i < len && spec[i] == ' ') {
                ++i;
            }
            if (i >= len) {
                break;
            }
            const size_t start = i;
            // 提取非空格字符作为字段内容
            while (i < len && spec[i] != ' ') {
                ++i;
            }
            fields[idx++].assign(spec, start, i - start);
        }

        // 跳过末尾的空格并验证字段数量
        while (i < len && spec[i] == ' ') {
            ++i;
        }
        if (idx != fields.size() || i < len) {
            throw std::invalid_argument("Cron规范正好需要5个字段");
        }

        // 分别解析分钟、小时、日期、月份和星期字段
        minutes_ = parseField(fields[0], 0, 59);
        hours_ = parseField(fields[1], 0, 23);
        days_ = parseField(fields[2], 1, 31);
        months_ = parseField(fields[3], 1, 12);
        weekdays_ = parseField(fields[4], 0, 6);
    }
    // 解析cron表达式中的单个字段，将其转换为具体数值列表
    // @param field - cron字段字符串，如"*"、"1-5"、"*/5"等
    // @param min - 该字段允许的最小值（包含）
    // @param max - 该字段允许的最大值（包含）
    // @return 包含所有有效值的有序向量
    // @throws std::invalid_argument 当字段为空或格式错误时抛出异常
    std::vector<int> CronExpr::parseField(const std::string &field, int min, int max) const
    {
        if (field.empty()) {
            throw std::invalid_argument("空cron字段");
        }

        const int range = max - min + 1;
        std::vector<uint8_t> seen(range, 0);

        size_t start = 0;
        while (start < field.size()) {
            // 按逗号分割字段，获取单个标记
            size_t end = field.find(',', start);
            const std::string token = field.substr(start, end == std::string::npos ? std::string::npos : end - start);
            if (token.empty()) {
                throw std::invalid_argument("无效的cron字段标记");
            }

            // 解析单个标记范围并标记已见过的值
            const auto values = parseRange(token, min, max);
            for (int v : values) {
                seen[v - min] = 1;
            }

            if (end == std::string::npos) {
                break;
            }
            start = end + 1;
        }

        // 将标记过的值转换为结果向量
        std::vector<int> result;
        result.reserve(range);
        for (int i = 0; i < range; ++i) {
            if (seen[i]) {
                result.push_back(min + i);
            }
        }
        if (result.empty()) {
            throw std::invalid_argument("cron字段不产生任何值");
        }
        return result;
    }
    std::vector<int> CronExpr::parseRange(const std::string &token, int min, int max) const
    {
        if (token.empty()) {
            throw std::invalid_argument("空cron范围");
        }
    
        // 解析步长值（如果存在）
        int step = 1;
        const size_t slash_pos = token.find('/');
        const std::string base = slash_pos == std::string::npos ? token : token.substr(0, slash_pos);
        if (slash_pos != std::string::npos) {
            if (slash_pos + 1 >= token.size()) {
                throw std::invalid_argument("无效的cron步骤");
            }
            step = std::stoi(token.substr(slash_pos + 1));
            if (step <= 0) {
                throw std::invalid_argument("cron步长必须为正");
            }
        }
    
        // 解析范围起始和结束值
        int range_start = min;
        int range_end = max;
    
        if (base != "*") {
            const size_t dash_pos = base.find('-');
            if (dash_pos == std::string::npos) {
                // 处理单一值情况，如"5"
                range_start = range_end = std::stoi(base);
            } else {
                // 处理范围值情况，如"1-5"
                if (dash_pos == 0 || dash_pos + 1 >= base.size()) {
                    throw std::invalid_argument("无效的cron范围");
                }
                range_start = std::stoi(base.substr(0, dash_pos));
                range_end = std::stoi(base.substr(dash_pos + 1));
            }
            if (range_start < min || range_end > max || range_start > range_end) {
                throw std::invalid_argument("cron范围超出界限");
            }
        }
    
        // 根据步长生成最终的值列表
        std::vector<int> result;
        result.reserve(static_cast<size_t>((range_end - range_start) / step) + 1);
        for (int v = range_start; v <= range_end; v += step) {
            result.push_back(v);
        }
    
        return result;
    }
}
