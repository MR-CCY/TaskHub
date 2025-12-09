#pragma once
#include "task_common.h"
#include <string>
#include <chrono>

namespace taskhub::core {

struct TaskResult {
    TaskStatus status{ 
        TaskStatus::Pending
     };
    std::string message;           // 错误信息、日志摘要等
    std::chrono::milliseconds duration{0};

    bool ok() const {
        return status == TaskStatus::Success;
    }
};

} // namespace taskhub::core