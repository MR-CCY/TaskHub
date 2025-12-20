#include "graph_model.h"

/**
 * @brief 向图中添加一个节点
 * @param n 要添加的节点数据，包含节点ID、类型和位置信息
 * @return 添加成功返回true，如果节点ID为空或节点已存在则返回false
 */
bool GraphModel::addNode(const NodeData& n) {
    if (n.id.isEmpty() || nodes_.contains(n.id)) return false;
    nodes_.insert(n.id, n);
    return true;
}

/**
 * @brief 设置指定节点的位置
 * @param id 节点ID
 * @param pos 新的位置坐标
 * @return 设置成功返回true，如果节点不存在则返回false
 */
bool GraphModel::setNodePos(const QString& id, const QPointF& pos) {
    if (!nodes_.contains(id)) return false;
    auto nd = nodes_[id];
    nd.pos = pos;
    nodes_[id] = nd;
    return true;
}

/**
 * @brief 检查两个节点之间是否存在边
 * @param from 起始节点ID
 * @param to 目标节点ID
 * @return 存在边返回true，否则返回false
 */
bool GraphModel::hasEdge(const QString& from, const QString& to) const {
    return edges_.contains(ek(from, to));
}

/**
 * @brief 在两个节点之间添加一条边
 * @param from 起始节点ID
 * @param to 目标节点ID
 * @return 添加成功返回true，以下情况会返回false：
 *         - 节点ID为空
 *         - 起始节点和目标节点相同
 *         - 起始节点或目标节点不存在
 *         - 边已经存在
 */
bool GraphModel::addEdge(const QString& from, const QString& to) {
    if (from.isEmpty() || to.isEmpty()) return false;
    if (from == to) return false;
    if (!nodes_.contains(from) || !nodes_.contains(to)) return false;
    const auto key = ek(from, to);
    if (edges_.contains(key)) return false;
    edges_.insert(key);
    return true;
}

/**
 * @brief 删除两个节点之间的边
 * @param from 起始节点ID
 * @param to 目标节点ID
 * @return 删除成功返回true，如果边不存在则返回false
 */
bool GraphModel::removeEdge(const QString& from, const QString& to) {
    const auto key = ek(from, to);
    if (!edges_.contains(key)) return false;
    edges_.remove(key);
    return true;
}

RemoveNodeResult GraphModel::removeNode(const QString &id)
{
    RemoveNodeResult r;
    if (!nodes_.contains(id)) return r;

    r.ok = true;
    r.removedNode = nodes_.value(id);
    nodes_.remove(id);

    // remove incident edges
    QVector<QString> toRemove;
    toRemove.reserve(edges_.size());

    for (const auto& key : edges_) {
        const int p = key.indexOf("->");
        if (p <= 0) continue;
        const QString from = key.left(p);
        const QString to   = key.mid(p + 2);
        if (from == id || to == id) {
            toRemove.push_back(key);
            r.removedEdges.push_back(EdgeData{from, to});
        }
    }

    for (const auto& k : toRemove) edges_.remove(k);
    return r;
}
