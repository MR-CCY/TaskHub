#include "workflow_scene.h"
#include "view/items/task_block_item.h"
#include "view/items/edge_item.h"

WorkflowScene::WorkflowScene(QObject* parent) : QGraphicsScene(parent) {}

TaskBlockItem* WorkflowScene::nodeItem(const QString& id) const {
    return nodes_.value(id, nullptr);
}

void WorkflowScene::onNodeAdded(const NodeData& n) {
    if (nodes_.contains(n.id)) return;

    auto* item = new TaskBlockItem(n.id);
    addItem(item);
    item->setPos(n.pos);

    connect(item, &TaskBlockItem::clicked, this, &WorkflowScene::nodeClicked);
    connect(item, &TaskBlockItem::dragFinished, this, &WorkflowScene::nodeDragFinished);

    nodes_.insert(n.id, item);
}

void WorkflowScene::onNodeMoved(const QString& id, const QPointF& pos) {
    auto* it = nodes_.value(id, nullptr);
    if (!it) return;
    it->setPos(pos);

    // update all edges touching this node (Q1-1 简化：扫一遍 edges_)
    for (auto eIt = edges_.begin(); eIt != edges_.end(); ++eIt) {
        const QString key = eIt.key(); // from->to
        const int p = key.indexOf("->");
        const QString from = key.left(p);
        const QString to   = key.mid(p + 2);
        if (from == id || to == id) refreshEdgeFor(from, to);
    }
}

void WorkflowScene::onEdgeAdded(const QString& from, const QString& to) {
    const QString key = ek(from, to);
    if (edges_.contains(key)) return;

    auto* e = new EdgeItem(from, to);
    addItem(e);
    edges_.insert(key, e);
    refreshEdgeFor(from, to);
}

void WorkflowScene::onEdgeRemoved(const QString& from, const QString& to) {
    const QString key = ek(from, to);
    auto* e = edges_.value(key, nullptr);
    if (!e) return;
    removeItem(e);
    delete e;
    edges_.remove(key);
}

void WorkflowScene::onNodeRemoved(const QString &id)
{
     // 1) remove edges touching this node
     QList<QString> edgeKeys;
     for (auto it = edges_.begin(); it != edges_.end(); ++it) {
         const QString key = it.key();
         const int p = key.indexOf("->");
         if (p <= 0) continue;
         const QString from = key.left(p);
         const QString to   = key.mid(p + 2);
         if (from == id || to == id) {
             edgeKeys.push_back(key);
         }
     }
     for (const auto& k : edgeKeys) {
         auto* e = edges_.value(k, nullptr);
         if (!e) continue;
         removeItem(e);
         delete e;
         edges_.remove(k);
     }
 
     // 2) remove node item
     auto* n = nodes_.value(id, nullptr);
     if (!n) return;
     removeItem(n);
     delete n;
     nodes_.remove(id);
}

void WorkflowScene::refreshEdgeFor(const QString& from, const QString& to) {
    auto* e = edges_.value(ek(from, to), nullptr);
    auto* a = nodes_.value(from, nullptr);
    auto* b = nodes_.value(to, nullptr);
    if (!e || !a || !b) return;

    const QPointF p1 = a->sceneBoundingRect().center();
    const QPointF p2 = b->sceneBoundingRect().center();
    e->updatePath(p1, p2);
}