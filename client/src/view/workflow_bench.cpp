#include "workflow_bench.h"

#include <QToolBar>
#include <QVBoxLayout>
#include <QAction>

#include "graph/graph_model.h"
#include "view/workflow_scene.h"
#include "view/workflow_canvas_view.h"
#include "commands/undostack.h"
#include "editor/editor_controller.h"

WorkflowBench::WorkflowBench(QWidget* parent)
    : QWidget(parent)
{
    // 1) Core objects
    model_ = new GraphModel();                 // GraphModel 不是 QObject 的话可以不用 parent
    scene_ = new WorkflowScene(this);
    view_  = new WorkflowCanvasView(this);
    undo_  = new UndoStack(this);

    view_->setScene(scene_);
    view_->setUndoStack(undo_);

    editor_ = new EditorController(*model_, *scene_, *undo_, this);

    // 2) UI + actions
    buildUi();
    wireActions();

    // 可选：给个大点的 scene rect，方便拖拽
    scene_->setSceneRect(-2000, -2000, 4000, 4000);
}

void WorkflowBench::buildUi()
{
    toolbar_ = new QToolBar(this);

    actAddNode_ = new QAction("Add Node", this);
    actUndo_    = new QAction("Undo", this);
    actRedo_    = new QAction("Redo", this);

    actUndo_->setShortcut(QKeySequence::Undo);
    actRedo_->setShortcut(QKeySequence::Redo);

    // 让快捷键在 bench 内生效（也可 addAction 到 MainWindow）
    addAction(actUndo_);
    addAction(actRedo_);

    toolbar_->addAction(actAddNode_);
    toolbar_->addAction(actUndo_);
    toolbar_->addAction(actRedo_);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0,0,0,0);
    lay->setSpacing(0);
    lay->addWidget(toolbar_);
    lay->addWidget(view_);
    setLayout(lay);
}

void WorkflowBench::wireActions()
{
    connect(actAddNode_, &QAction::triggered, this, &WorkflowBench::addNodeShell);
    connect(actUndo_, &QAction::triggered, this, &WorkflowBench::undo);
    connect(actRedo_, &QAction::triggered, this, &WorkflowBench::redo);

    // 可选：根据 undo 状态刷新按钮可用性
    actUndo_->setEnabled(false);
    actRedo_->setEnabled(false);
    connect(undo_, &UndoStack::changed, this, [this]{
        actUndo_->setEnabled(undo_->canUndo());
        actRedo_->setEnabled(undo_->canRedo());
        
        emit debugUndoStateChanged(
            undo_->canUndo(),
            undo_->canRedo(),
            undo_->undoText(),
            undo_->redoText()
        );
    });

}

void WorkflowBench::addNodeShell()
{
    editor_->addNode("shell"); // 你已有的 EditorController::addNode(type)
}

void WorkflowBench::undo()
{
    undo_->undo();
}

void WorkflowBench::redo()
{
    undo_->redo();
}

