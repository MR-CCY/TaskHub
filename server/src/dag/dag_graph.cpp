#include "dag_graph.h"
namespace taskhub::dag {
    DagNode::Ptr DagGraph::getNode(const core::TaskId &id) const
    {
        auto it = _nodes.find(id.value);
        if (it == _nodes.end()) {
            return nullptr;
        }
        return it->second;
    }

    DagNode::Ptr DagGraph::ensureNode(const core::TaskId &id)
    {
        auto it = _nodes.find(id.value);
        if (it != _nodes.end()) {
            return it->second;
        }

        auto node = std::make_shared<DagNode>(id);
        _nodes.emplace(id.value, node);
        return node;
    }

    const DagGraph::NodeMap &DagGraph::nodes() const noexcept
    {
        return _nodes;
    }

    void DagGraph::recomputeIndegree()
    {
        for (auto& [id, node] : _nodes) {
            node->setIndegree(static_cast<int>(node->dependencies().size()));
        }
    }

} // namespace name
