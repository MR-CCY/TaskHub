#pragma once
#include <string>
#include "json.hpp"

namespace taskhub {

    enum class TaskStatus {
        Pending = 0,
        Running = 1,
        Success = 2,
        Failed  = 3
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
   static std::string status_to_string(TaskStatus s) {
        switch (s) {
            case TaskStatus::Pending: return "pending";
            case TaskStatus::Running: return "running";
            case TaskStatus::Success: return "success";
            case TaskStatus::Failed:  return "failed";
        }
        return "unknown";
    }

    std::string to_string() {
        return "Task{id=" + std::to_string(id) +
               ", name=" + name +
               ", type=" + std::to_string(type) +
               ", status=" + status_to_string(status) +
               ", create_time=" + create_time +
               ", update_time=" + update_time +
               ", params=" + params.dump() +
               "}";
    };

  
};

} // namespace taskhub