// CmdAddEdge.cpp
#include "cmd_add_edge.h"
#include "graph/graph_model.h"
#include "view/graph_view_adapter.h"

CmdAddEdge::CmdAddEdge(GraphModel& m, GraphViewAdapter& v, QString f, QString t)
    : model_(m), view_(v), from_(std::move(f)), to_(std::move(t)) {}

bool CmdAddEdge::redo() {
    if (applied_) return true;
    if (!model_.addEdge(from_, to_)) return false;
    view_.onEdgeAdded(from_, to_);
    applied_ = true;
    return true;
}

void CmdAddEdge::undo() {
    if (!applied_) return;
    model_.removeEdge(from_, to_);
    view_.onEdgeRemoved(from_, to_);
    applied_ = false;
}