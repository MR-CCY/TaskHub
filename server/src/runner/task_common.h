#pragma once
#include <string>

namespace taskhub::core {

    enum class TaskStatus {
        Pending,   // 0
        Running,   // 1
        Success,   // 2
        Failed,    // 3
        Skipped,   // 4
        Canceled,  // 5
        Timeout    // 6
    };

struct TaskId {
    std::string value;
    bool empty() const { return value.empty(); }
    bool operator==(const TaskId& other) const { return value == other.value; }
    bool operator<(const TaskId& other) const { return value < other.value; }
};

} // namespace taskhub::core
