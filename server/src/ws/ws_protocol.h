#pragma once
#include <string>
#include <unordered_set>
#include "json.hpp"
#include "log/log_record.h"
#include "utils/utils.h"
using json = nlohmann::json;
namespace taskhub::ws {

// ---------- 基础枚举 ----------

// 客户端指令类型
enum class WsOp {
    Subscribe,
    Unsubscribe,
    Ping,
    Pong,
    Unknown
};

// 订阅主题
enum class WsTopic {
    TaskLogs = 0,
    TaskEvents = 1,
    Unknown = 2
};

// ---------- 客户端指令 ----------

struct ClientCommand {
    WsOp op{WsOp::Unknown};
    WsTopic topic{WsTopic::Unknown};

    // 目前只支持 task_id 级别
    std::string taskId;
};

// ---------- 服务端推送 ----------

struct ServerMessage {
    WsTopic topic;
    std::string taskId;

    // 实际 payload（日志 / 事件）
    json data;
};

// ---------- 协议函数（只声明） ----------

// 工具函数
inline std::string toString(WsTopic topic){
    switch (topic) {
        case WsTopic::TaskEvents:
            return "task_events";
        case WsTopic::TaskLogs:
            return "task_logs";
        default:
            return "unknown";
    }
};
inline std::string toString(WsOp op){
    switch (op) {
        case WsOp::Subscribe:
            return "subscribe";
        case WsOp::Unsubscribe:
            return "unsubscribe";
        case WsOp::Ping:
            return "ping";
        case WsOp::Pong:
            return "pong";
        default:
            return "unknown";
    }
};

inline WsOp parseOpField(const json& j, const char* key) {
    try {
        auto it = j.find(key);
        if (it == j.end()) return WsOp::Unknown;
        if (it->is_string()) {
            const auto s = it->get<std::string>();
            if (s == "subscribe") return WsOp::Subscribe;
            if (s == "unsubscribe") return WsOp::Unsubscribe;
            if (s == "ping") return WsOp::Ping;
            if (s == "pong") return WsOp::Pong;
            return WsOp::Unknown;
        }
        if (it->is_number_integer()) {
            switch (it->get<int>()) {
                case static_cast<int>(WsOp::Subscribe):   return WsOp::Subscribe;
                case static_cast<int>(WsOp::Unsubscribe): return WsOp::Unsubscribe;
                case static_cast<int>(WsOp::Ping):        return WsOp::Ping;
                case static_cast<int>(WsOp::Pong):        return WsOp::Pong;
                default:                                  return WsOp::Unknown;
            }
        }
    } catch (...) {
        return WsOp::Unknown;
    }
    return WsOp::Unknown;
}

inline WsTopic parseTopicField(const json& j, const char* key) {
    try {
        auto it = j.find(key);
        if (it == j.end()) return WsTopic::Unknown;
        if (it->is_string()) {
            const auto s = it->get<std::string>();
            if (s == "task_logs") return WsTopic::TaskLogs;
            if (s == "task_events") return WsTopic::TaskEvents;
            return WsTopic::Unknown;
        }
        if (it->is_number_integer()) {
            switch (it->get<int>()) {
                case static_cast<int>(WsTopic::TaskLogs):   return WsTopic::TaskLogs;
                case static_cast<int>(WsTopic::TaskEvents): return WsTopic::TaskEvents;
                default:                                    return WsTopic::Unknown;
            }
        }
    } catch (...) {
        return WsTopic::Unknown;
    }
    return WsTopic::Unknown;
}

// 解析客户端 JSON → ClientCommand
inline ClientCommand parseClientCommand(const json& j){
    ClientCommand cmd;
    cmd.op = parseOpField(j, "op");
    cmd.topic = parseTopicField(j, "topic");
    if (auto it = j.find("task_id"); it != j.end() && it->is_string()) {
        cmd.taskId = it->get<std::string>();
    }
    return cmd;
};

// 构造服务端推送 JSON
inline json buildServerMessage(const ServerMessage& msg){
    return json{
        {"topic", toString(msg.topic)},
        {"task_id", msg.taskId},
        {"data", msg.data}
    };
};

inline std::string channelTaskLogs(const std::string& taskId) {
    return "task.logs." + taskId;     // 订阅某个 task 的日志
}

inline std::string channelTaskEvents(const std::string& taskId) {
    return "task.events." + taskId;   // 订阅某个 task 的事件（预留）
}

inline json buildLogJson(const core::LogRecord& r) {
    json j;
    j["type"]      = "log";
    j["task_id"]   = r.taskId.value;
    j["seq"]       = r.seq;
    j["ts_ms"]     = utils::now_millis();
    j["level"]     = static_cast<int>(r.level);
    j["stream"]    = static_cast<int>(r.stream);
    j["message"]   = r.message;
    j["duration_ms"] = r.durationMs;
    j["attempt"]   = r.attempt;

    // fields
    json jf = json::object();
    for (auto& kv : r.fields) jf[kv.first] = kv.second;
    j["fields"] = jf;

    return j;
}
inline json buildTaskEventJson(const std::string& taskId,
    const std::string& event,
    const json& extra) {
    json j;
    j["type"]    = "event";
    j["task_id"] = taskId;
    j["event"]   = event;
    j["ts_ms"]   = utils::now_millis();
    j["extra"]   = extra;
    return j;
}

} // namespace taskhub::ws
