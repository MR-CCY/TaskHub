// DagNode.h
#pragma once
#include <vector>
#include <atomic>
#include <memory>
#include <functional>
#include "runner/task_common.h"
#include "runner/taskRunner.h"
#include "runner/task_config.h"
namespace taskhub::dag {

class DagNode {
public:
    using Ptr = std::shared_ptr<DagNode>;

    DagNode(const core::TaskId& id);

    const core::TaskId& id() const noexcept;

    // 依赖管理
    const std::vector<core::TaskId>& dependencies() const noexcept;
    const std::vector<core::TaskId>& downstream() const noexcept;

    void addDependency(const core::TaskId& dep);
    void addDownstream(const core::TaskId& child);

    // indegree 用于拓扑调度（运行时用）
    int indegree() const noexcept;
    void setIndegree(int v) noexcept;
    int decrementIndegree() noexcept;

    // 状态
    core::TaskStatus status() const noexcept;
    void setStatus(core::TaskStatus s) noexcept;

    // 绑定具体任务执行配置
    void setRunnerConfig(const core::TaskConfig& cfg);
    const core::TaskConfig& runnerConfig() const;

private:
    core::TaskId _id;
    std::vector<core::TaskId> _deps;
    std::vector<core::TaskId> _downstream;
    std::atomic<int> _indegree{0};
    std::atomic<core::TaskStatus> _status{ core::TaskStatus::Pending };
    core::TaskConfig _runnerCfg;
};

} // namespace taskhub::dag
