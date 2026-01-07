#include "line_item_factory.h"

#include "arrow_line_item.h"
#include "line_item.h"
#include "rect_item.h"
#include "container_rect_item.h"

LineItem* LineItemFactory::createLine(RectItem* start, RectItem* end, QGraphicsItem* parent) {
    // 目前统一返回带箭头、加粗的线
    return new ArrowLineItem(start, end, parent);
}

bool LineItemFactory::canConnect(RectItem* start, RectItem* end, QGraphicsItem*& outParent) {
    outParent = nullptr;
    if (!start || !end || start == end) return false;
    auto* pa = dynamic_cast<ContainerRectItem*>(start->parentItem());
    auto* pb = dynamic_cast<ContainerRectItem*>(end->parentItem());
    if (pa != pb) return false;
    outParent = pa;
    return true;
}
