// CmdAddNode.cpp
#include "cmd_add_node.h"
#include "graph/graph_model.h"
#include "view/graph_view_adapter.h"

CmdAddNode::CmdAddNode(GraphModel& m, GraphViewAdapter& v, NodeData n)
    : model_(m), view_(v), node_(std::move(n)) {}

bool CmdAddNode::redo() {
    if (applied_) return true;
    if (!model_.addNode(node_)) return false;
    view_.onNodeAdded(node_);
    applied_ = true;
    return true;
}

void CmdAddNode::undo() {
    if (!applied_) return;

    auto rr = model_.removeNode(node_.id);
    // rr.removedEdges 正常情况下应该为空：因为 undo 顺序会先撤销边，再撤销节点
    view_.onNodeRemoved(node_.id);

    applied_ = false;
}