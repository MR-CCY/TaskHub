#pragma once

class RectItem;
class LineItem;
class QGraphicsItem;

class LineItemFactory {
public:
    static LineItem* createLine(RectItem* start, RectItem* end, QGraphicsItem* parent = nullptr);
    static bool canConnect(RectItem* start, RectItem* end, QGraphicsItem*& outParent);
};
