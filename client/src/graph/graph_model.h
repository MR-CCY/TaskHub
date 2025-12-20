#pragma once
#include "graph_types.h"
#include <QHash>
#include <QSet>

struct RemoveNodeResult {
    bool ok = false;
    NodeData removedNode;
    QVector<EdgeData> removedEdges; // incident edges（Q1-1.1 基本为空，但模型层负责兜底）
};

class GraphModel {
public:
    bool hasNode(const QString& id) const { return nodes_.contains(id); }
    NodeData node(const QString& id) const { return nodes_.value(id); }

    bool addNode(const NodeData& n);
    bool setNodePos(const QString& id, const QPointF& pos);

    bool hasEdge(const QString& from, const QString& to) const;
    bool addEdge(const QString& from, const QString& to);
    bool removeEdge(const QString& from, const QString& to);
    RemoveNodeResult removeNode(const QString& id);
private:
    static QString ek(const QString& f, const QString& t) { return f + "->" + t; }

    QHash<QString, NodeData> nodes_;
    QSet<QString> edges_;
};