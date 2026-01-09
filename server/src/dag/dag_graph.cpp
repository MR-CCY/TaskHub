#include "dag_graph.h"
#include "log/logger.h"
namespace taskhub::dag {
    // 辅助函数：生成复合 key
    static std::string makeKey(const core::TaskId& id) {
        if (id.runId.empty()) {
            return id.value;
        }
        return id.runId + ":" + id.value;
    }

    DagNode::Ptr DagGraph::getNode(const core::TaskId &id) const
    {
        auto key = makeKey(id);
        auto it = _nodes.find(key);
        
        if (it == _nodes.end()) {
            Logger::warn("DagGraph::getNode - node not found: " + key);
            return nullptr;
        }
        return it->second;
    }

    DagNode::Ptr DagGraph::ensureNode(const core::TaskId &id)
    {
        auto key = makeKey(id);
        auto it = _nodes.find(key);
        if (it != _nodes.end()) {
            return it->second;
        }

        auto node = std::make_shared<DagNode>(id);
        _nodes.emplace(key, node);
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
