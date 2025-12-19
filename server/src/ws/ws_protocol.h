#pragma once
#include <string>
#include <unordered_set>
#include "json.hpp"
#include "log/log_record.h"
#include "utils/utils.h"
using json = nlohmann::json;
namespace taskhub::ws {

// WebSocket 协议（ws_server_beast + WsLogStreamer）
// 1）握手与鉴权：连接后首条消息必须带 token，形如 {"token":"<jwt>"}。
//    - 只发 token（无 op）会收到 {"type":"authed"}，后续再发送订阅指令。
//    - 也可在首条消息里同时带 op/ topic（下面 2、3）。
// 2）订阅/退订日志或事件：
//    {"token":"<jwt>","op":"subscribe","topic":"task_logs","task_id":"t1","run_id":"r1?"}
//    {"token":"<jwt>","op":"unsubscribe","topic":"task_events","task_id":"t1","run_id":"r1?"}
//    topic 取值 task_logs / task_events，task_id 必填，run_id 选填。
//    对应频道名：
//      task_logs   → task.logs.<task_id>[.<run_id>]
//      task_events → task.events.<task_id>[.<run_id>]
// 3）心跳：{"token":"<jwt>","op":"ping"}，服务端回复 {"type":"pong"}。
// 4）服务端推送（订阅频道才会收到）：
//    - 日志：{"type":"log","task_id":"t1","run_id":"r1?","seq":1,"ts_ms":<ms>,"level":int,"stream":int,"message":"...","duration_ms":<ms>,"attempt":1,"fields":{...}}
//    - 事件：{"type":"event","task_id":"t1","run_id":"r1?","event":"task_start","ts_ms":<ms>,"extra":{...}}
// 5）全局广播（无需订阅，直接所有 session 收到）：
//    例如 broadcast_task_event 发送 {"event":"task_updated","data":{task...}}，
//    SystemHandler::broadcast 发送 {"event":"debug","data":{"msg":"..."}}。

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
    std::string runId;
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
    if (auto it = j.find("run_id"); it != j.end() && it->is_string()) {
        cmd.runId = it->get<std::string>();
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

inline std::string channelTaskLogs(const std::string& taskId, const std::string& runId = "") {
    if (runId.empty()) return "task.logs." + taskId;
    return "task.logs." + taskId + "." + runId;     // 订阅某个 task 的日志
}

inline std::string channelTaskEvents(const std::string& taskId, const std::string& runId = "") {
    if (runId.empty()) return "task.events." + taskId;
    return "task.events." + taskId + "." + runId;   // 订阅某个 task 的事件（预留）
}

inline json buildLogJson(const core::LogRecord& r) {
    json j;
    j["type"]      = "log";
    j["task_id"]   = r.taskId.value;
    if (!r.taskId.runId.empty()) j["run_id"] = r.taskId.runId;
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
    const json& extra,
    const std::string& runId = "") {
    json j;
    j["type"]    = "event";
    j["task_id"] = taskId;
    if (!runId.empty()) j["run_id"] = runId;
    j["event"]   = event;
    j["ts_ms"]   = utils::now_millis();
    j["extra"]   = extra;
    return j;
}

} // namespace taskhub::ws
