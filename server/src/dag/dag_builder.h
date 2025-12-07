// DagBuilder.h
#pragma once
#include <vector>
#include <string>
#include <optional>
#include "dag_graph.h"
#include "dag_types.h"

namespace taskhub::dag {

/// 构图时传入的任务描述（来自配置或上层 API）
struct DagTaskSpec {
    core::TaskId id;
    std::vector<core::TaskId> deps;
    core::TaskConfig runnerCfg;
};

/// 校验结果
struct DagValidateResult {
    bool ok{true};
    std::string errorMessage;
    std::vector<core::TaskId> cycleNodes; // 如检测到环，把相关节点列出来
};

class DagBuilder {
public:
    DagBuilder() = default;

    // 添加一个任务节点描述
    void addTask(const DagTaskSpec& spec);

    // 生成最终的图
    DagGraph build();

    // 单独对构建结果做校验（是否有环、依赖是否存在）
    DagValidateResult validate() const;

private:
    std::vector<DagTaskSpec> _specs;
};

} // namespace taskhub::dag