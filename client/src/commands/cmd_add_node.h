// CmdAddNode.h
#pragma once
#include "commands/command.h"
#include "graph/graph_types.h"

class GraphModel;
class GraphViewAdapter;

class CmdAddNode : public ICommand {
public:
    CmdAddNode(GraphModel& m, GraphViewAdapter& v, NodeData n);

    QString name() const override { return "AddNode"; }
    bool redo() override;
    void undo() override;

private:
    GraphModel& model_;
    GraphViewAdapter& view_;
    NodeData node_;
    bool applied_ = false;
};