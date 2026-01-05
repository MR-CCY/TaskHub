// DagRunContext.h
#pragma once
#include <atomic>
#include <functional>
#include <mutex>
#include <unordered_map>
#include "dag_graph.h"
#include "dag_types.h"
#include "runner/task_runner.h"
#include "map"
namespace taskhub::dag {

/// 对外回调接口，用于 UI / 日志
struct DagEventCallbacks {
    // 某个节点状态变化
    std::function<void(const core::TaskId&, core::TaskStatus)> onNodeStatusChanged;
    // 整个 DAG 完成（成功 / 失败）
    std::function<void(bool success)> onDagFinished;
};

class DagRunContext {
public:
    DagRunContext(const DagConfig& cfg,
                  DagGraph graph,
                  DagEventCallbacks callbacks);

    DagConfig config() const { return _config; }
    DagGraph& graph() { return _graph; }
    const DagGraph& graph() const { return _graph; }

    // 用于统计 / 控制
    void incrementRunning();
    void decrementRunning();
    int runningCount() const;

    void markFailed();        // 记录 DAG 已经进入失败状态
    bool isFailed() const;

    // 状态上报：内部调用时自动触发回调
    void setNodeStatus(const core::TaskId& id, core::TaskStatus st);
    void setTaskResults(const core::TaskId& id, core::TaskResult);
    std::map<core::TaskId, core::TaskResult> taskResults();
    // 最终结束时调用
    void finish(bool success);
    //返回总结
    // std::map<core::TaskId, core::TaskStatus> finalStatus();
private:
    DagConfig _config;
    DagGraph _graph;
    DagEventCallbacks _callbacks;

    std::atomic<int> _running{0};
    std::atomic_bool _failed{false};

    mutable std::mutex _mutex; // 保护必要的共享数据（如回调调用时的顺序）
    // std::map<core::TaskId, core::TaskStatus> _finalStatus;
    std::map<core::TaskId, core::TaskResult> _taskResults;
    core::TaskResult _finalResults;
};

} // namespace taskhub::dag