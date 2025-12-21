// shell_rect_item.h
#pragma once
#include "rect_item.h"

class ShellRectItem : public RectItem {
public:
    explicit ShellRectItem(const QRectF& rect, QGraphicsItem* parent = nullptr);

    QString typeLabel() const override;
    QColor headerColor() const override;
    QString summaryText() const override;
};