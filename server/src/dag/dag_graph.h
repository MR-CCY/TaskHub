// DagGraph.h
#pragma once
#include <unordered_map>
#include "dag_node.h"
#include "dag_types.h"

namespace taskhub::dag {

class DagGraph {
public:
    using NodeMap = std::unordered_map<std::string, DagNode::Ptr>;

    DagGraph() = default;

    DagNode::Ptr getNode(const core::TaskId& id) const;
    DagNode::Ptr ensureNode(const core::TaskId& id);

    const NodeMap& nodes() const noexcept;

    // 在 build 阶段调用：根据 deps 填充 indegree
    void recomputeIndegree();

private:
    NodeMap _nodes;
};

} // namespace taskhub::dag