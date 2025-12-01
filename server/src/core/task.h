#pragma once
#include <string>
#include "json.hpp"

namespace taskhub {

    enum class TaskStatus {
        Pending = 0,
        Running = 1,
        Success = 2,
        Failed  = 3,
        Canceled =4,
        Timeout =5
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
            case TaskStatus::Canceled: return "canceled";
            case TaskStatus::Timeout: return "timeout";
        }
        return "unknown";
     }

    static  TaskStatus  string_to_status(const std::string s){  
      if(s=="pending"){
        return TaskStatus::Pending;
      }else if(s=="running"){
        return TaskStatus::Running;
      }else if(s=="success"){
        return TaskStatus::Success;
      }else if(s=="canceled"){
        return TaskStatus::Canceled;
      }else if(s=="timeout"){
        return TaskStatus::Timeout;
      }else{
        return TaskStatus::Failed;
      }

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

    int         exit_code{0};        // 命令返回码
    std::string last_output;         // 命令输出（stdout）
    std::string last_error;          // 执行失败的错误信息（如 cmd 缺失）
    
    int         max_retries   = 0;   // 最大重试次数，比如 0 表示不重试
    int         retry_count   = 0;   // 已重试次数
    int         timeout_sec   = 0;   // 超时时间，0 表示不限制
    bool        cancel_flag   = false; // 是否请求取消
};

} // namespace taskhub