#pragma once
#include "container_rect_item.h"

class DagRectItem : public ContainerRectItem {
public:
    explicit DagRectItem(const QRectF& rect, QGraphicsItem* parent = nullptr);

    QString typeLabel() const override;
    QColor headerColor() const override;
    QString summaryText() const override;
};
