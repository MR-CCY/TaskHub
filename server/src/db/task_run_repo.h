#pragma once

#include <string>
#include "json.hpp"
#include "runner/task_result.h"

namespace taskhub {

class TaskRunRepo {
public:
    static TaskRunRepo& instance();

    void upsertFromTaskJson(const std::string& runId, const nlohmann::json& jtask);
    void markRunning(const std::string& runId, const std::string& logicalId, long long tsMs);
    void markFinished(const std::string& runId, const std::string& logicalId, const core::TaskResult& r, long long tsMs);
    void markSkipped(const std::string& runId, const std::string& logicalId, const std::string& reason, long long tsMs);

    struct TaskRunRow {
        int id{0};
        std::string runId;
        std::string logicalId;
        std::string taskId;
        std::string name;
        std::string execType;
        std::string execCommand;
        std::string execParamsJson;
        std::string depsJson;
        int status{0};
        int exitCode{0};
        long long durationMs{0};
        std::string message;
        std::string stdoutText;
        std::string stderrText;
        int attempt{0};
        int maxAttempts{0};
        long long startTsMs{0};
        long long endTsMs{0};
        std::string workerId;
        std::string workerHost;
        int workerPort{0};
    };

    std::vector<TaskRunRow> query(const std::string& runId,const std::string& name,int limit);
    
    // 获取指定 run_id 下某个逻辑节点的运行记录
    std::optional<TaskRunRow> get(const std::string& runId, const std::string& logicalId);

private:
    TaskRunRepo() = default;
    int statusToInt(core::TaskStatus st) const;
};

} // namespace taskhub
