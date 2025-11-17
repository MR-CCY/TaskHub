#pragma once
#include <string>
#include "json.hpp"

namespace taskhub {

enum class TaskStatus {
    Pending,   // 已创建，待执行
    Running,   // （M3 用）
    Success,   // （M3 用）
    Failed     // （M3 用）
};

struct Task {
    using IdType = std::uint64_t;

    IdType      id{};
    std::string name;
    int         type{0};        // 任务类型，先留整数位，后面扩展用
    TaskStatus  status{TaskStatus::Pending};

    std::string create_time;    // 简单用字符串存 ISO 时间
    std::string update_time;

    // 任意参数，由前端/客户端传进来
    nlohmann::json params;
    std::string to_string() const{
        return "Task{id=" + std::to_string(id) +
               ", name=" + name +
               ", type=" + std::to_string(type) +
               ", status=" + std::to_string(static_cast<int>(status)) +
               ", create_time=" + create_time +
               ", update_time=" + update_time +
               ", params=" + params.dump() +
               "}";
    };
};

} // namespace taskhub