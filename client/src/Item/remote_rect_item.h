// remote_rect_item.h
#pragma once
#include "rect_item.h"

class RemoteRectItem : public RectItem {
public:
    explicit RemoteRectItem(const QRectF& rect, QGraphicsItem* parent = nullptr);

    QString typeLabel() const override;
    QColor headerColor() const override;
    QString summaryText() const override;
};