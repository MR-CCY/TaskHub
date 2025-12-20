// CmdMoveNode.h
#pragma once
#include "commands/command.h"
#include <QString>
#include <QPointF>

class GraphModel;
class GraphViewAdapter;

class CmdMoveNode : public ICommand {
public:
    CmdMoveNode(GraphModel& m, GraphViewAdapter& v, QString id, QPointF oldPos, QPointF newPos);

    QString name() const override { return "MoveNode"; }
    bool redo() override;
    void undo() override;

private:
    GraphModel& model_;
    GraphViewAdapter& view_;
    QString id_;
    QPointF old_;
    QPointF now_;
};