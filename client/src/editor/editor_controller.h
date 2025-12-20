#pragma once
#include <QObject>
#include <QString>

class GraphModel;
class WorkflowScene;
class UndoStack;
class ConnectTask;
class SelectTask;

class EditorController : public QObject {
    Q_OBJECT
public:
    EditorController(GraphModel& model, WorkflowScene& scene, UndoStack& undo, QObject* parent=nullptr);

    void addNode(const QString& type);

private:
    GraphModel& model_;
    WorkflowScene& scene_;
    UndoStack& undo_;
    int nodeCounter_ = 0;

    ConnectTask* connectTask_;
    SelectTask* selectTask_;
};