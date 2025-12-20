#pragma once
#include <QGraphicsScene>
#include <QHash>

#include "view/graph_view_adapter.h"

class TaskBlockItem;
class EdgeItem;

class WorkflowScene : public QGraphicsScene, public GraphViewAdapter {
    Q_OBJECT
public:
    explicit WorkflowScene(QObject* parent=nullptr);

    // GraphViewAdapter
    void onNodeAdded(const NodeData& n) override;
    void onNodeMoved(const QString& id, const QPointF& pos) override;
    void onEdgeAdded(const QString& from, const QString& to) override;
    void onEdgeRemoved(const QString& from, const QString& to) override;
    void onNodeRemoved(const QString& id) override;
    
    TaskBlockItem* nodeItem(const QString& id) const;

signals:
    void nodeClicked(const QString& nodeId);
    void nodeDragFinished(const QString& nodeId, const QPointF& oldPos, const QPointF& newPos);

private:
    void refreshEdgeFor(const QString& from, const QString& to);

private:
    QHash<QString, TaskBlockItem*> nodes_;
    QHash<QString, EdgeItem*> edges_; // key "from->to"
    static QString ek(const QString& f, const QString& t) { return f + "->" + t; }
};