#pragma once
#include "graph/graph_types.h"

class GraphViewAdapter {
public:
    virtual ~GraphViewAdapter() = default;

    virtual void onNodeAdded(const NodeData& n) = 0;
    virtual void onNodeMoved(const QString& id, const QPointF& pos) = 0;
    virtual void onNodeRemoved(const QString& id) = 0;
    
    virtual void onEdgeAdded(const QString& from, const QString& to) = 0;
    virtual void onEdgeRemoved(const QString& from, const QString& to) = 0;
};