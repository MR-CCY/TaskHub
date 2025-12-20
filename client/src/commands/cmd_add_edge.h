// CmdAddEdge.h
#pragma once
#include "commands/command.h"
#include <QString>

class GraphModel;
class GraphViewAdapter;

class CmdAddEdge : public ICommand {
public:
    CmdAddEdge(GraphModel& m, GraphViewAdapter& v, QString from, QString to);

    QString name() const override { return "AddEdge"; }
    bool redo() override;
    void undo() override;

private:
    GraphModel& model_;
    GraphViewAdapter& view_;
    QString from_;
    QString to_;
    bool applied_ = false;
};