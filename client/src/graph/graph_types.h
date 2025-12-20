#pragma once
#include <QString>
#include <QPointF>

struct NodeData {
    QString id;
    QString type;
    QPointF pos;
};

struct EdgeData {
    QString from;
    QString to;
};