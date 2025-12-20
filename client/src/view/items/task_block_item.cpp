#include "task_block_item.h"
#include <QPainter>
#include <QGraphicsSceneMouseEvent>

TaskBlockItem::TaskBlockItem(QString nodeId) : nodeId_(std::move(nodeId)) {
    setFlags(QGraphicsItem::ItemIsMovable |
             QGraphicsItem::ItemIsSelectable |
             QGraphicsItem::ItemSendsGeometryChanges);
}

QRectF TaskBlockItem::boundingRect() const { return QRectF(0, 0, 120, 60); }

void TaskBlockItem::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) {
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setPen(isSelected() ? Qt::blue : Qt::black);
    p->setBrush(QColor(240, 240, 240));
    p->drawRoundedRect(boundingRect(), 6, 6);
    p->drawText(boundingRect().adjusted(8, 8, -8, -8), nodeId_);
}

void TaskBlockItem::mousePressEvent(QGraphicsSceneMouseEvent* e) {
    pressed_ = true;
    pressPos_ = pos();
    QGraphicsObject::mousePressEvent(e);
}

void TaskBlockItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* e) {
    QGraphicsObject::mouseReleaseEvent(e);

    if (!pressed_) return;
    pressed_ = false;

    emit clicked(nodeId_);
    const QPointF newPos = pos();
    if (newPos != pressPos_) {
        emit dragFinished(nodeId_, pressPos_, newPos);
    }
}