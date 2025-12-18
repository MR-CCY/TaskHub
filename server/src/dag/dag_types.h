#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include "runner/task_result.h"

namespace taskhub::dag {

/// DAG 失败策略：某节点失败后如何处理整个 DAG
enum class FailPolicy {
    FailFast,       // 立即终止整个 DAG
    SkipDownstream, // 跳过所有依赖该节点的下游节点
};

/// DAG 运行配置
struct DagConfig {
    FailPolicy failPolicy{ FailPolicy::SkipDownstream };
    std::uint32_t maxParallel{ 4 }; // 最大并行度（可约束线程池并发）
};
struct DagResult
{
    bool success{false};
    std::string message;
    std::vector<core::TaskId> taskIds;
    std::map<core::TaskId, core::TaskResult> taskResults;
    json to_json() const {
        json j = json::array();
        for(const auto& id : taskIds){
            auto it = taskResults.find(id);
            if (it != taskResults.end()) {
                j.push_back(core::taskResultToJson(it->second));
            }
        }
        return j;
    }
};


} // namespace taskhub::dag