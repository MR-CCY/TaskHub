#pragma once
#include "container_rect_item.h"

class TemplateRectItem : public ContainerRectItem {
public:
    explicit TemplateRectItem(const QRectF& rect, QGraphicsItem* parent = nullptr);

    QString typeLabel() const override;
    QColor headerColor() const override;
    QString summaryText() const override;
};
