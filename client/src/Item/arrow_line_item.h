#pragma once

#include "line_item.h"

class ArrowLineItem : public LineItem {
public:
    ArrowLineItem(RectItem* start, RectItem* end, QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};
