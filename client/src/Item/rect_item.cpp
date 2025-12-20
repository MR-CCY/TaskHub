#include "rect_item.h"

#include "line_item.h"

QVariant RectItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == ItemPositionHasChanged) {
        // 当我移动时，通知所有连线更新
        for (auto* line : lines_) {
            line->trackNodes();
        }
    }
    return BaseItem::itemChange(change, value);
}

void RectItem::addLine(LineItem* line) {
    lines_.insert(line);
}

void RectItem::removeLine(LineItem* line) {
    lines_.remove(line);
}
