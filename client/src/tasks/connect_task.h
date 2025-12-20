// ConnectTask.h
#pragma once
#include <QString>

class GraphModel;
class GraphViewAdapter;
class UndoStack;

class ConnectTask {
public:
    ConnectTask(GraphModel& m, GraphViewAdapter& v, UndoStack& u);

    void onNodeClicked(const QString& nodeId);

private:
    GraphModel& model_;
    GraphViewAdapter& view_;
    UndoStack& undo_;
    QString pendingSource_;
};