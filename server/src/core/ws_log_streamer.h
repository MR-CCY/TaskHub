#pragma once
#include "core/log_record.h"
#include "json.hpp"
using json = nlohmann::json;
namespace taskhub::ws {

class WsLogStreamer {
public:
    static WsLogStreamer& instance();

    // 推送一条日志（M12.2 核心）
    void pushLog(const core::LogRecord& record);

    // 预留：推送任务级事件（后面用）
    void pushTaskEvent(const std::string& taskId,
                       const std::string& event,
                       const json& extra = {});
private:
    WsLogStreamer() = default;
};

} // namespace taskhub::ws