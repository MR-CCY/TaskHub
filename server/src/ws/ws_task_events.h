#pragma once

#include "ws/ws_hub.h"
#include "utils/utils.h"          // 为了 now_string，如果你用到了
#include "core/task_manager.h"   // 或定义 Task 的头文件
#include "json.hpp"
#include <string>
#include "core/task.h"
#include "log/logger.h"
namespace taskhub {

using nlohmann::json;


// 把 Task 转成 JSON（尽量和 /api/tasks 返回结构保持一致）
inline json task_to_json(const Task& t) {
    json j;
    j["id"]          = t.id;
    j["name"]        = t.name;
    j["type"]        = t.type;
    j["params"]      = t.params;  // 如果是 json 字段就直接给
    j["status"]      = Task::status_to_string(t.status);
    j["create_time"] = t.create_time;
    j["update_time"] = t.update_time;
    j["exit_code"]   = t.exit_code;
    j["last_output"] = t.last_output;
    j["last_error"]  = t.last_error;
    return j;
}

// WebSocket 全局广播格式：{"event":<event_name>,"data":{task...}}，无需订阅即可收到。
// 统一封装：发送任务相关事件
inline void broadcast_task_event(const std::string& event, const Task& t) {
    json wrapper;
    wrapper["event"] = event;          // 如 "task_created" / "task_updated"
    wrapper["data"]  = task_to_json(t);

    std::string payload;
    try {
        // 避免非 UTF-8 内容导致 dump 直接抛异常
        payload = wrapper.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
    } catch (const std::exception& ex) {
        Logger::error(std::string("broadcast_task_event dump failed: ") + ex.what());
        return;
    }

    try {
        WsHub::instance().broadcast(payload);
    } catch (const std::exception& ex) {
        Logger::error(std::string("broadcast_task_event send failed: ") + ex.what());
    }
}

} // namespace taskhub
