// SelectTask.cpp
#include "select_task.h"
#include "commands/cmd_move_node.h"
#include "commands/undostack.h"
#include <memory>

SelectTask::SelectTask(GraphModel& m, GraphViewAdapter& v, UndoStack& u)
    : model_(m), view_(v), undo_(u) {}

void SelectTask::onNodeDragFinished(const QString& nodeId, const QPointF& oldPos, const QPointF& newPos) {
    if (oldPos == newPos) return;
    undo_.push(std::make_unique<CmdMoveNode>(model_, view_, nodeId, oldPos, newPos));
}