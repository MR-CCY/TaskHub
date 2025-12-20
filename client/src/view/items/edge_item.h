#pragma once
#include <QGraphicsPathItem>
#include <QString>

class EdgeItem : public QGraphicsPathItem {
public:
    EdgeItem(QString from, QString to);

    QString from() const { return from_; }
    QString to() const { return to_; }

    void updatePath(const QPointF& p1, const QPointF& p2);

private:
    QString from_;
    QString to_;
};