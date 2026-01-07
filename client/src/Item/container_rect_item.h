#pragma once
#include "rect_item.h"

class ContainerRectItem : public RectItem {
public:
    explicit ContainerRectItem(const QRectF& rect, QGraphicsItem* parent = nullptr);

    QRectF contentRect() const;
    void adjustToChildren();

    QString typeLabel() const override;
    QColor headerColor() const override;
    QString summaryText() const override;

private:
    QRectF defaultRect_;
};
