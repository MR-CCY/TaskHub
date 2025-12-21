// local_rect_item.h
#pragma once
#include "rect_item.h"

class LocalRectItem : public RectItem {
public:
    explicit LocalRectItem(const QRectF& rect, QGraphicsItem* parent = nullptr);

    QString typeLabel() const override;
    QColor headerColor() const override;
    QString summaryText() const override;
};