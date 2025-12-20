#include "base_item.h"

BaseItem::BaseItem(Kind k, QGraphicsItem* parent)
    : QGraphicsObject(parent), kind_(k) {}

void BaseItem::attachContext(QGraphicsScene* s, GraphModel* m, UndoStack* u) {
    scene_ = s;
    model_ = m;
    undo_  = u;
}
