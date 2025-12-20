// ConnectTask.cpp
#include "connect_task.h"
#include "graph/graph_model.h"
#include "commands/undostack.h"
#include "commands/cmd_add_edge.h"
#include <memory>

ConnectTask::ConnectTask(GraphModel& m, GraphViewAdapter& v, UndoStack& u)
    : model_(m), view_(v), undo_(u) {}

void ConnectTask::onNodeClicked(const QString& nodeId) {
    if (pendingSource_.isEmpty()) {
        pendingSource_ = nodeId;
        return;
    }
    const QString from = pendingSource_;
    const QString to   = nodeId;
    pendingSource_.clear();

    undo_.push(std::make_unique<CmdAddEdge>(model_, view_, from, to));
}