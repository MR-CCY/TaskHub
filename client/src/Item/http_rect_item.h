// http_rect_item.h
#pragma once
#include "rect_item.h"

class HttpRectItem : public RectItem {
public:
    explicit HttpRectItem(const QRectF& rect, QGraphicsItem* parent = nullptr);

    QString typeLabel() const override;
    QColor headerColor() const override;
    QString summaryText() const override;
};