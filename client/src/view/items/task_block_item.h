#pragma once
#include <QGraphicsObject>
#include <QString>

class TaskBlockItem : public QGraphicsObject {
    Q_OBJECT
public:
    explicit TaskBlockItem(QString nodeId);

    QString nodeId() const { return nodeId_; }

    QRectF boundingRect() const override;
    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override;

signals:
    void clicked(const QString& nodeId);
    void dragFinished(const QString& nodeId, const QPointF& oldPos, const QPointF& newPos);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* e) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* e) override;

private:
    QString nodeId_;
    QPointF pressPos_;
    bool pressed_ = false;
};