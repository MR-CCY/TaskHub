// container_rect_item.cpp
#include "container_rect_item.h"

#include <QGraphicsItem>
#include <QVector>

ContainerRectItem::ContainerRectItem(const QRectF& rect, QGraphicsItem* parent)
    : RectItem(rect, parent), defaultRect_(rect)
{
}

QRectF ContainerRectItem::contentRect() const {
    QRectF inner = rect().adjusted(pad_, headerH_ + pad_, -pad_, -pad_);
    if (inner.width() <= 0 || inner.height() <= 0) {
        inner = rect();
    }
    return inner;
}

void ContainerRectItem::adjustToChildren() {
    QVector<RectItem*> nodes;
    const auto kids = childItems();
    nodes.reserve(kids.size());
    for (auto* child : kids) {
        auto* node = dynamic_cast<RectItem*>(child);
        if (!node) continue;
        if (!node->scene()) continue;
        nodes.append(node);
    }

    if (nodes.isEmpty()) {
        setRect(defaultRect_);
        return;
    }

    bool first = true;
    qreal minLeft = 0.0;
    qreal maxRight = 0.0;
    qreal minTop = 0.0;
    qreal maxBottom = 0.0;

    for (auto* node : nodes) {
        QRectF r = node->rect().translated(node->pos());
        if (first) {
            minLeft = r.left();
            maxRight = r.right();
            minTop = r.top();
            maxBottom = r.bottom();
            first = false;
            continue;
        }
        if (r.left() < minLeft) {
            minLeft = r.left();
        }
        if (r.right() > maxRight) {
            maxRight = r.right();
        }
        if (r.top() < minTop) {
            minTop = r.top();
        }
        if (r.bottom() > maxBottom) {
            maxBottom = r.bottom();
        }
    }

    // 使用固定留白：标准节点尺寸（150）的一半 = 75
    constexpr qreal PADDING = 75.0;
    const qreal left = minLeft - PADDING;
    const qreal right = maxRight + PADDING;
    const qreal top = minTop - PADDING;
    const qreal bottom = maxBottom + PADDING;
    QRectF outer(left, top, right - left, bottom - top);

    if (outer.width() <= 0 || outer.height() <= 0) {
        setRect(defaultRect_);
        return;
    }

    setRect(outer);

    if (auto* parentContainer = dynamic_cast<ContainerRectItem*>(parentItem())) {
        if (parentContainer->scene()) {
            parentContainer->adjustToChildren();
        }
    }
}

QString ContainerRectItem::typeLabel() const { return "BOX"; }
QColor  ContainerRectItem::headerColor() const { return QColor(90, 90, 90); }
QString ContainerRectItem::summaryText() const { return ""; }
