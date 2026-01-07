#include "create_command.h"

#include "Item/container_rect_item.h"
#include "Item/rect_item.h"

CreateCommand::CreateCommand(QGraphicsScene* scene, QGraphicsItem* item, QUndoCommand* parent)
    : BaseCommand("Create Item", parent), scene_(scene), item_(item)
{
    parent_ = item_ ? item_->parentItem() : nullptr;
    if (item_) {
        pos_ = item_->pos();
    }
}

CreateCommand::~CreateCommand() {
    if (item_ && !item_->scene()) {
        delete item_;
    }
}

void CreateCommand::execute() {
    if (item_ && parent_ && item_->parentItem() != parent_) {
        item_->setParentItem(parent_);
    }
    if (item_) {
        item_->setPos(pos_);
    }
    if (item_ && !item_->scene()) {
        scene_->addItem(item_);
    }
    adjustParentContainer();
}

void CreateCommand::unExecute() {
    if (item_ && item_->scene()) {
        scene_->removeItem(item_);
    }
    adjustParentContainer();
}

void CreateCommand::adjustParentContainer() {
    auto* rect = dynamic_cast<RectItem*>(item_);
    if (!rect) return;
    auto* container = dynamic_cast<ContainerRectItem*>(rect->parentItem());
    if (!container) return;
    if (container->scene()) {
        container->adjustToChildren();
    }
}
