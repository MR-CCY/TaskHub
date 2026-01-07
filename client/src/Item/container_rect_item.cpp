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
        if (dynamic_cast<ContainerRectItem*>(node)) continue;
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
    qreal leftWidth = 0.0;
    qreal rightWidth = 0.0;
    qreal topHeight = 0.0;
    qreal bottomHeight = 0.0;

    for (auto* node : nodes) {
        QRectF r = node->rect().translated(node->pos());
        if (first) {
            minLeft = r.left();
            maxRight = r.right();
            minTop = r.top();
            maxBottom = r.bottom();
            leftWidth = r.width();
            rightWidth = r.width();
            topHeight = r.height();
            bottomHeight = r.height();
            first = false;
            continue;
        }
        if (r.left() < minLeft) {
            minLeft = r.left();
            leftWidth = r.width();
        }
        if (r.right() > maxRight) {
            maxRight = r.right();
            rightWidth = r.width();
        }
        if (r.top() < minTop) {
            minTop = r.top();
            topHeight = r.height();
        }
        if (r.bottom() > maxBottom) {
            maxBottom = r.bottom();
            bottomHeight = r.height();
        }
    }

    const qreal left = minLeft - leftWidth / 2.0;
    const qreal right = maxRight + rightWidth / 2.0;
    const qreal top = minTop - topHeight / 2.0;
    const qreal bottom = maxBottom + bottomHeight / 2.0;
    QRectF outer(left, top, right - left, bottom - top);

    if (outer.width() <= 0 || outer.height() <= 0) {
        setRect(defaultRect_);
        return;
    }

    setRect(outer);
}

QString ContainerRectItem::typeLabel() const { return "BOX"; }
QColor  ContainerRectItem::headerColor() const { return QColor(90, 90, 90); }
QString ContainerRectItem::summaryText() const { return ""; }
