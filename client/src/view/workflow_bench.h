
#pragma once
#include <QWidget>

class QToolBar;
class QAction;

class GraphModel;
class WorkflowScene;
class WorkflowCanvasView;
class UndoStack;
class EditorController;

class WorkflowBench : public QWidget {
    Q_OBJECT
public:
    explicit WorkflowBench(QWidget* parent = nullptr);

    // 如果你希望 MainWindow 的菜单栏复用这些 action
    QAction* actionAddNode() const { return actAddNode_; }
    QAction* actionUndo() const { return actUndo_; }
    QAction* actionRedo() const { return actRedo_; }

    // 可选：对外暴露少量 API（后面 Q1-3/Q1-4/Q1-6 会用）
    void addNodeShell(); // demo
    void undo();
    void redo();
signals:
    void debugUndoStateChanged(bool canUndo, bool canRedo, QString undoTop, QString redoTop);
private:
    void buildUi();
    void wireActions();

private:
    // --- core objects (生命周期跟 Bench) ---
    GraphModel* model_ = nullptr;
    WorkflowScene* scene_ = nullptr;
    WorkflowCanvasView* view_ = nullptr;
    UndoStack* undo_ = nullptr;
    EditorController* editor_ = nullptr;

    // --- UI ---
    QToolBar* toolbar_ = nullptr;
    QAction* actAddNode_ = nullptr;
    QAction* actUndo_ = nullptr;
    QAction* actRedo_ = nullptr;
};