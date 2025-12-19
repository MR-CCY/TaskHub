#pragma once
#include <algorithm>
#include <cctype>
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
    std::string runId;
    bool empty() const { return value.empty(); }
    bool operator==(const TaskId& other) const {
        if (runId.empty() && other.runId.empty()) return value == other.value;
        return value == other.value && runId == other.runId;
    }
    bool operator<(const TaskId& other) const {
        if (value == other.value) return runId < other.runId;
        return value < other.value;
    }
};
inline std::string TaskStatusTypetoString(TaskStatus type){
   switch (type)
   {
        case TaskStatus::Pending: return "Pending";
        case TaskStatus::Running: return "Running";
        case TaskStatus::Success: return "Success";
        case TaskStatus::Failed: return "Failed";
        case TaskStatus::Skipped: return "Skipped";
        case TaskStatus::Canceled: return "Canceled";
        case TaskStatus::Timeout: return "Timeout";
        default: return "Unknown";
   }
};
inline TaskStatus TaskExecTypetoStatus(std::string str){
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    
   if (str == "unknown") return TaskStatus::Failed;
   if (str == "pending") return TaskStatus::Pending;
   if (str == "running") return TaskStatus::Running;
   if (str == "success") return TaskStatus::Success;
   if (str == "failed") return TaskStatus::Failed;
   if (str == "skipped") return TaskStatus::Skipped;
   if (str == "canceled") return TaskStatus::Canceled;
   if (str == "timeout") return TaskStatus::Timeout;
   return TaskStatus::Failed; // default
};

} // namespace taskhub::core
