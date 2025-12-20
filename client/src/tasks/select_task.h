// SelectTask.h
#pragma once
#include <QString>
#include <QPointF>

class GraphModel;
class GraphViewAdapter;
class UndoStack;

class SelectTask {
public:
    SelectTask(GraphModel& m, GraphViewAdapter& v, UndoStack& u);

    void onNodeDragFinished(const QString& nodeId, const QPointF& oldPos, const QPointF& newPos);

private:
    GraphModel& model_;
    GraphViewAdapter& view_;
    UndoStack& undo_;
};