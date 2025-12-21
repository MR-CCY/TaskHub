#include "line_item_factory.h"

#include "arrow_line_item.h"
#include "line_item.h"

LineItem* LineItemFactory::createLine(RectItem* start, RectItem* end, QGraphicsItem* parent) {
    // 目前统一返回带箭头、加粗的线
    return new ArrowLineItem(start, end, parent);
}
