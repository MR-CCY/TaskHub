#pragma once
#include "rect_item.h"

class TemplateRectItem : public RectItem {
public:
    explicit TemplateRectItem(const QRectF& rect, QGraphicsItem* parent = nullptr);

    QString typeLabel() const override;
    QColor headerColor() const override;
    QString summaryText() const override;
};
