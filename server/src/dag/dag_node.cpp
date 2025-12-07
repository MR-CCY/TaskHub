#include "dag_node.h"
namespace  taskhub::dag{
    DagNode::DagNode(const core::TaskId &id):
    _id(id)
    {

    }

    const core::TaskId &DagNode::id() const noexcept
    {
        // TODO: 在此处插入 return 语句
        return _id;
    }
    const std::vector<core::TaskId>& DagNode::dependencies() const noexcept
    {
        return _deps;
    }
    const std::vector<core::TaskId>& DagNode::downstream() const noexcept
    {
        return _downstream;
    }
    void DagNode::addDependency(const core::TaskId &dep)
    {
        _deps.push_back(dep);
    }
    void DagNode::addDownstream(const core::TaskId &child)
    {
        _downstream.push_back(child);
    }
    int DagNode::indegree() const noexcept
    {
        return _indegree.load(std::memory_order_relaxed);
    }
    void DagNode::setIndegree(int v) noexcept
    {
        _indegree.store(v, std::memory_order_relaxed);
    }
    int DagNode::decrementIndegree() noexcept
    {
        int old = _indegree.fetch_sub(1, std::memory_order_relaxed);
        // debug 下可以：
        // assert(old > 0 && "indegree underflow");
        return old - 1; // 如果你采用“返回新值”的语义
    }
    core::TaskStatus DagNode::status() const noexcept
    {
        return _status.load(std::memory_order_relaxed);
    }
    void DagNode::setStatus(core::TaskStatus s) noexcept
    {
        _status.store(s, std::memory_order_relaxed);
    }
    void DagNode::setRunnerConfig(const core::TaskConfig &cfg)
    {
        _runnerCfg = cfg;
    }
    const core::TaskConfig &DagNode::runnerConfig() const
    {
        // TODO: 在此处插入 return 语句
        return _runnerCfg;
    }
}
