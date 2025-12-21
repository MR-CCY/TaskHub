#pragma once

#include "node_type.h"

#include <QRectF>
#include <QString>

class QGraphicsItem;
class RectItem;

class NodeItemFactory {
public:
    static RectItem* createNode(NodeType type, const QRectF& rect, QGraphicsItem* parent = nullptr);
    static NodeType fromString(const QString& value);
    static QString toString(NodeType type);
};
