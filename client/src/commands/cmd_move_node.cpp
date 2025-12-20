// CmdMoveNode.cpp
#include "cmd_move_node.h"
#include "graph/graph_model.h"
#include "view/graph_view_adapter.h"

CmdMoveNode::CmdMoveNode(GraphModel& m, GraphViewAdapter& v, QString id, QPointF o, QPointF n)
    : model_(m), view_(v), id_(std::move(id)), old_(o), now_(n) {}

bool CmdMoveNode::redo() {
    if (!model_.setNodePos(id_, now_)) return false;
    view_.onNodeMoved(id_, now_);
    return true;
}

void CmdMoveNode::undo() {
    model_.setNodePos(id_, old_);
    view_.onNodeMoved(id_, old_);
}