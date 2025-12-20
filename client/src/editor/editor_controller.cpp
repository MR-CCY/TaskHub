#include "editor_controller.h"
#include "graph/graph_model.h"
#include "view/workflow_scene.h"
#include "commands/undostack.h"
#include "commands/cmd_add_node.h"
#include "tasks/connect_task.h"
#include "tasks/select_task.h"

#include <memory>

EditorController::EditorController(GraphModel& model, WorkflowScene& scene, UndoStack& undo, QObject* parent)
    : QObject(parent), model_(model), scene_(scene), undo_(undo) {

    connectTask_ = new ConnectTask(model_, scene_, undo_);
    selectTask_  = new SelectTask(model_, scene_, undo_);

    connect(&scene_, &WorkflowScene::nodeClicked, this, [this](const QString& id){
        connectTask_->onNodeClicked(id);
    });
    connect(&scene_, &WorkflowScene::nodeDragFinished, this,
            [this](const QString& id, const QPointF& oldP, const QPointF& newP){
        selectTask_->onNodeDragFinished(id, oldP, newP);
    });
}

void EditorController::addNode(const QString& type) {
    ++nodeCounter_;
    const QString id = QString("N%1").arg(nodeCounter_);
    NodeData n;
    n.id = id;
    n.type = type;
    n.pos = QPointF(50 * nodeCounter_, 50 * nodeCounter_);

    undo_.push(std::make_unique<CmdAddNode>(model_, scene_, n));
}