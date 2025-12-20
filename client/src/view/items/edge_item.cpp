#include "edge_item.h"
#include <QPainterPath>
#include <QPen>

EdgeItem::EdgeItem(QString f, QString t) : from_(std::move(f)), to_(std::move(t)) {
    setZValue(-1);
    setPen(QPen(Qt::black, 2));
}

void EdgeItem::updatePath(const QPointF& p1, const QPointF& p2) {
    QPainterPath path;
    path.moveTo(p1);
    const QPointF c1(p1.x() + 80, p1.y());
    const QPointF c2(p2.x() - 80, p2.y());
    path.cubicTo(c1, c2, p2);
    setPath(path);
}