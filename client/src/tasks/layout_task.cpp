#include "layout_task.h"

#include <QHash>
#include <QMap>
#include <QQueue>
#include <QSet>
#include <QString>
#include <QVector>
#include <QtGlobal>
#include <algorithm>

#include "Item/container_rect_item.h"
#include "Item/line_item.h"
#include "Item/rect_item.h"
#include "view/canvasscene.h"
#include "view/canvasview.h"
namespace {
struct NodeInfo {
    QSet<RectItem*> parents;
    QSet<RectItem*> children;
};

QString nodeKey(RectItem* node) {
    const QString id = node->prop("id").toString();
    if (!id.isEmpty()) return id;
    return QString::number(reinterpret_cast<quintptr>(node), 16);
}

void layoutNodes(const QVector<RectItem*>& nodes, const QVector<LineItem*>& lines) {
    if (nodes.isEmpty()) return;
    
    // 布局期间禁用碰撞检测
    RectItem::setCollisionDetectionEnabled(false);

    const auto isContainer = [](RectItem* node) {
        return dynamic_cast<ContainerRectItem*>(node) != nullptr;
    };

    // 使用固定的标准节点尺寸，而不是从第一个节点推断
    constexpr qreal baseNodeHeight = 150.0;
    constexpr qreal baseNodeWidth = 150.0;

    auto slotCount = [&](RectItem* node) {
        if (!isContainer(node)) return 1;
        const qreal h = node->rect().height();
        if (h <= 0.0) return 1;
        int slotCountValue = qCeil(h / baseNodeHeight);
        return qBound(1, slotCountValue, 5);
    };

    QHash<RectItem*, NodeInfo> info;
    info.reserve(nodes.size());
    for (auto* node : nodes) {
        info.insert(node, NodeInfo());
    }

    for (auto* line : lines) {
        RectItem* src = line->startItem();
        RectItem* dst = line->endItem();
        if (!src || !dst) continue;
        if (!info.contains(src) || !info.contains(dst)) continue;
        info[src].children.insert(dst);
        info[dst].parents.insert(src);
    }

    QHash<RectItem*, int> indegree;
    QHash<RectItem*, int> level;
    QQueue<RectItem*> queue;

    int maxLevel = 1;
    for (auto* node : nodes) {
        const int deg = info[node].parents.size();
        indegree[node] = deg;
        if (deg == 0) {
            level[node] = 1;
            queue.enqueue(node);
        }
    }

    while (!queue.isEmpty()) {
        RectItem* node = queue.dequeue();
        const int base = level.value(node, 1);
        for (auto* child : info[node].children) {
            const int nextLevel = base + 1;
            if (level.value(child, 1) < nextLevel) {
                level[child] = nextLevel;
                maxLevel = std::max(maxLevel, nextLevel);
            }
            indegree[child] = indegree.value(child) - 1;
            if (indegree[child] == 0) {
                queue.enqueue(child);
            }
        }
    }

    for (auto* node : nodes) {
        if (!level.contains(node)) {
            level[node] = 1;
        }
        maxLevel = std::max(maxLevel, level[node]);
    }

    QMap<int, QVector<RectItem*>> buckets;
    for (auto* node : nodes) {
        buckets[level.value(node, 1)].append(node);
    }

    QMap<int, QVector<RectItem*>> finalLevels;
    const int maxSlotsPerLevel = 4;
    auto effectiveSlots = [&](RectItem* node) {
        return std::min(slotCount(node), maxSlotsPerLevel);
    };

    auto childCount = [&](RectItem* n) { return info[n].children.size(); };
    auto displayCmp = [&](RectItem* a, RectItem* b) {
        const int ca = childCount(a);
        const int cb = childCount(b);
        const bool aHas = ca > 0;
        const bool bHas = cb > 0;
        if (aHas != bHas) return aHas > bHas;
        if (ca != cb) return ca > cb;
        return nodeKey(a) < nodeKey(b);
    };
    auto pushCmp = [&](RectItem* a, RectItem* b) {
        const int ca = childCount(a);
        const int cb = childCount(b);
        const bool aHas = ca > 0;
        const bool bHas = cb > 0;
        if (aHas != bHas) return aHas < bHas;
        if (ca != cb) return ca < cb;
        return nodeKey(a) < nodeKey(b);
    };

    for (int lvl = 1; lvl <= maxLevel; ++lvl) {
        QVector<RectItem*> nodesAt = buckets.value(lvl);
        if (nodesAt.isEmpty()) continue;

        QVector<RectItem*> display = nodesAt;
        std::sort(display.begin(), display.end(), displayCmp);

        int totalSlots = 0;
        for (auto* node : display) {
            totalSlots += effectiveSlots(node);
        }
        if (totalSlots > maxSlotsPerLevel) {
            QVector<RectItem*> pushList = nodesAt;
            std::sort(pushList.begin(), pushList.end(), pushCmp);

            QSet<RectItem*> overflowSet;
            for (auto* node : pushList) {
                if (totalSlots <= maxSlotsPerLevel) break;
                overflowSet.insert(node);
                totalSlots -= effectiveSlots(node);
            }

            QVector<RectItem*> kept;
            kept.reserve(display.size());
            for (auto* node : display) {
                if (!overflowSet.contains(node)) kept.append(node);
            }

            QVector<RectItem*> overflowNodes;
            overflowNodes.reserve(overflowSet.size());
            for (auto* node : pushList) {
                if (overflowSet.contains(node)) overflowNodes.append(node);
            }

            const int nextLevel = lvl + 1;
            for (auto* node : overflowNodes) {
                buckets[nextLevel].append(node);
            }
            maxLevel = std::max(maxLevel, nextLevel);
            display = kept;
        }

        finalLevels[lvl] = display;
    }

    // 使用标准节点尺寸计算布局间距
    const qreal spacingX = 260.0;  // 列间距
    const qreal minGapX = 75.0;    // 最小水平间距：标准节点宽度的一半
    const qreal spacingY = 220.0;  // 行间距
    qreal colX = 0.0;
    for (auto it = finalLevels.begin(); it != finalLevels.end(); ++it) {
        const auto& list = it.value();
        qreal maxWidth = 0.0;
        for (auto* node : list) {
            maxWidth = std::max(maxWidth, node->rect().width());
        }
        if (maxWidth <= 0.0) {
            maxWidth = baseNodeWidth;
        }
        int rowSlots = 0;
        for (auto* node : list) {
            QPointF pos(colX, rowSlots * spacingY);
            node->setPos(pos);
            QString nodeId = node->prop("id").toString();
            QRectF nodeRect = node->rect();
            qDebug() << "[Layout] Node:" << nodeId << "pos:" << pos << "rect:" << nodeRect << "size:" << nodeRect.width() << "x" << nodeRect.height();
            rowSlots += slotCount(node);
        }
        const qreal stepX = std::max(spacingX, maxWidth + minGapX);
        colX += stepX;
    }
    
    // 调试：列出所有顶层节点
    qDebug() << "[Layout] === Layout completed, listing all top-level nodes ===";
    for (auto* node : nodes) {
        QString nodeId = node->prop("id").toString();
        QPointF scenePos = node->scenePos();
        QRectF sceneBounds = node->mapRectToScene(node->boundingRect());
        bool hasParent = (node->parentItem() != nullptr);
        qDebug() << "[Layout] Node:" << nodeId << "scenePos:" << scenePos << "sceneBounds:" << sceneBounds << "hasParent:" << hasParent;
    }
    
    // 恢复碰撞检测
    RectItem::setCollisionDetectionEnabled(true);
}
}

LayoutTask::LayoutTask(CanvasScene* scene, CanvasView* view, QObject* parent)
    : Task(10, parent), scene_(scene) {
    setView(view);  // 设置 view，避免后续 view() 返回空指针
}

bool LayoutTask::dispatch(QEvent* e) {
    Q_UNUSED(e);
    return false;
}

void LayoutTask::execute() {
    if (!scene_) {
        removeSelf();
        return;
    }

    QVector<LineItem*> allLines;
    QVector<ContainerRectItem*> containers;
    QVector<RectItem*> topLevelNodes;

    const auto allItems = scene_->items();
    allLines.reserve(allItems.size());
    containers.reserve(allItems.size());
    topLevelNodes.reserve(allItems.size());

    for (auto* item : allItems) {
        if (auto* line = dynamic_cast<LineItem*>(item)) {
            allLines.append(line);
            continue;
        }
        if (auto* container = dynamic_cast<ContainerRectItem*>(item)) {
            containers.append(container);
            if (!container->parentItem()) {
                topLevelNodes.append(container);
            }
            continue;
        }
        if (auto* node = dynamic_cast<RectItem*>(item)) {
            if (!node->parentItem()) {
                topLevelNodes.append(node);
            }
        }
    }

    const auto containerDepth = [](const ContainerRectItem* c) -> int {
        int d = 0;
        for (auto* p = c ? c->parentItem() : nullptr; p; p = p->parentItem()) {
            if (dynamic_cast<ContainerRectItem*>(p)) {
                ++d;
            }
        }
        return d;
    };

    std::sort(containers.begin(), containers.end(),
              [&](const ContainerRectItem* a, const ContainerRectItem* b) {
                  return containerDepth(a) > containerDepth(b); // deep-first
              });

    for (auto* container : containers) {
        QVector<RectItem*> childNodes;
        QSet<RectItem*> childSet;
        const auto children = container->childItems();
        childNodes.reserve(children.size());
        for (auto* child : children) {
            auto* node = dynamic_cast<RectItem*>(child);
            if (!node) continue;
            if (!node->scene()) continue;
            childNodes.append(node);
            childSet.insert(node);
        }
        if (!childNodes.isEmpty()) {
            QVector<LineItem*> childLines;
            childLines.reserve(allLines.size());
            for (auto* line : allLines) {
                if (childSet.contains(line->startItem()) && childSet.contains(line->endItem())) {
                    childLines.append(line);
                }
            }
            layoutNodes(childNodes, childLines);
        }
        container->adjustToChildren();
    }

    if (!topLevelNodes.isEmpty()) {
        QSet<RectItem*> topLevelSet(topLevelNodes.begin(), topLevelNodes.end());
        QVector<LineItem*> topLevelLines;
        topLevelLines.reserve(allLines.size());
        for (auto* line : allLines) {
            if (topLevelSet.contains(line->startItem()) && topLevelSet.contains(line->endItem())) {
                topLevelLines.append(line);
            }
        }
        layoutNodes(topLevelNodes, topLevelLines);
    }

    // 关键：使用实际节点的边界，而不是scene的固定sceneRect(-5000,-5000,10000,10000)
    QRectF itemsRect = view()->scene()->itemsBoundingRect();
    if (!itemsRect.isEmpty()) {
        // 加一些边距让内容不贴边
        qreal margin = 50.0;
        itemsRect.adjust(-margin, -margin, margin, margin);
        
        // 基于实际内容范围自动缩放并居中
        view()->fitInView(itemsRect, Qt::KeepAspectRatio);
        // 稍微放大让节点更清晰
        view()->scale(25, 25);
    }
    removeSelf();
}
