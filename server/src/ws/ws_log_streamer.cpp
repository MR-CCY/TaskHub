#include "ws_log_streamer.h"
#include "utils/utils.h"
#include "ws/ws_protocol.h"
#include "ws/ws_hub.h"

namespace taskhub::ws {
WsLogStreamer &WsLogStreamer::instance()
{
    // TODO: 在此处插入 return 语句
    static WsLogStreamer instance;
    return instance;
}

void WsLogStreamer::pushLog(const core::LogRecord &record)
{
   // 推送格式：{"type":"log","task_id":"t1","run_id":"r1?","seq":1,"ts_ms":<ms>,"level":int,"stream":int,"message":"...","duration_ms":<ms>,"attempt":1,"fields":{...}}
   const std::string ch = channelTaskLogs(record.taskId.value);  // task.logs.<task_id>
   const std::string chRun = record.taskId.runId.empty() ? "" : channelTaskLogs(record.taskId.value, record.taskId.runId); // task.logs.<task_id>.<run_id>

   json j = buildLogJson(record);

   // 只推送到订阅了对应 channel 的 session；不全局广播，避免噪音
   try {
       auto payload = j.dump(-1, ' ', false, json::error_handler_t::replace); // replace 非 UTF-8
       if (!chRun.empty()) {
           WsHub::instance().broadcast(chRun, payload);
       }
       WsHub::instance().broadcast(ch, payload); // 兼容 run_id 空的订阅
   } catch (...) {
       // 避免日志推送异常杀死进程；忽略本条推送
   }
}
void WsLogStreamer::pushTaskEvent(const std::string &taskId, const std::string &event, const json &extra)
{
    const std::string runId = extra.value("run_id", std::string{});
    const std::string ch    = channelTaskEvents(taskId);
    const std::string chRun = runId.empty() ? "" : channelTaskEvents(taskId, runId);
    // 推送格式：{"type":"event","task_id":"t1","run_id":"r1?","event":"task_start","ts_ms":<ms>,"extra":{...}}
    json j = buildTaskEventJson(taskId, event, extra, runId);
    auto payload = j.dump();
    if (!chRun.empty()) {
        WsHub::instance().broadcast(chRun, payload);
    }
    WsHub::instance().broadcast(ch, payload);
}
}
