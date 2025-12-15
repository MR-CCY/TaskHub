#include "ws_log_streamer.h"
#include "core/utils.h"
#include "core/ws_protocol.h"
#include "core/ws_hub.h"

namespace taskhub::ws {
WsLogStreamer &WsLogStreamer::instance()
{
    // TODO: 在此处插入 return 语句
    static WsLogStreamer instance;
    return instance;
}

void WsLogStreamer::pushLog(const core::LogRecord &record)
{
   // TODO-1: 只推 task_id 相关的 channel（先不做全局广播，避免噪音）
   const std::string ch = channelTaskLogs(record.taskId.value);

   // TODO-2: LogRecord -> JSON
   json j = buildLogJson(record);

   // TODO-3: 广播给订阅者
   WsHub::instance().broadcast(ch, j.dump());
}
void WsLogStreamer::pushTaskEvent(const std::string &taskId, const std::string &event, const json &extra)
{
    const std::string ch = channelTaskEvents(taskId);
    json j = buildTaskEventJson(taskId, event, extra);
    WsHub::instance().broadcast(ch, j.dump());
}
}