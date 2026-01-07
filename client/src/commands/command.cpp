#include "command.h"

#include "Item/line_item.h"
#include "Item/rect_item.h"
#include "Item/container_rect_item.h"

namespace {
void adjustContainersForItems(const QSet<QGraphicsItem*>& items) {
    QSet<ContainerRectItem*> containers;
    for (auto* item : items) {
        auto* rect = dynamic_cast<RectItem*>(item);
        if (!rect) continue;
        auto* container = dynamic_cast<ContainerRectItem*>(rect->parentItem());
        if (container) {
            containers.insert(container);
        }
    }
    for (auto* container : containers) {
        if (container->scene()) {
            container->adjustToChildren();
        }
    }
}

void adjustContainersForParents(const QHash<QGraphicsItem*, QGraphicsItem*>& parents) {
    QSet<ContainerRectItem*> containers;
    for (auto it = parents.begin(); it != parents.end(); ++it) {
        auto* container = dynamic_cast<ContainerRectItem*>(it.value());
        if (container) {
            containers.insert(container);
        }
    }
    for (auto* container : containers) {
        if (container->scene()) {
            container->adjustToChildren();
        }
    }
}

void adjustParentContainer(RectItem* item) {
    if (!item) return;
    auto* container = dynamic_cast<ContainerRectItem*>(item->parentItem());
    if (!container) return;
    if (container->scene()) {
        container->adjustToChildren();
    }
}
}

BaseCommand::BaseCommand(const QString& text, QUndoCommand* parent)
    : QUndoCommand(text, parent) {}

void BaseCommand::redo() { execute(); }
void BaseCommand::undo() { unExecute(); }

void BaseCommand::pushTo(UndoStack* stack) {
    if (stack) {
        stack->push(this);
    } else {
        execute();
        delete this;
    }
}

CreateConnectionCommand::CreateConnectionCommand(QGraphicsScene* scene, LineItem* line, QUndoCommand* parent)
    : BaseCommand("Connect", parent), scene_(scene), line_(line) {}

CreateConnectionCommand::~CreateConnectionCommand() {
    if (line_ && !line_->scene()) delete line_;
}

void CreateConnectionCommand::execute() {
    if (!line_->scene()) scene_->addItem(line_);
}

void CreateConnectionCommand::unExecute() {
    if (line_->scene()) scene_->removeItem(line_);
}

MoveRectCommand::MoveRectCommand(RectItem* item, QPointF oldPos, QPointF newPos)
    : BaseCommand("Move Rect"), item_(item), oldPos_(oldPos), newPos_(newPos) {}

void MoveRectCommand::execute() {
    item_->setPos(newPos_);
    adjustParentContainer(item_);
}
void MoveRectCommand::unExecute() {
    item_->setPos(oldPos_);
    adjustParentContainer(item_);
}

AdjustLineCommand::AdjustLineCommand(LineItem* item, QPointF oldOff, QPointF newOff)
    : BaseCommand("Adjust Line"), item_(item), oldOff_(oldOff), newOff_(newOff) {}

void AdjustLineCommand::execute() { item_->setControlOffset(newOff_); }
void AdjustLineCommand::unExecute() { item_->setControlOffset(oldOff_); }

DeleteCommand::DeleteCommand(QGraphicsScene* scene, const QSet<QGraphicsItem*>& items)
    : BaseCommand("Delete"), scene_(scene), items_(items) {
    for (auto* item : items_) {
        parents_[item] = item->parentItem();
        positions_[item] = item->pos();
    }
}

DeleteCommand::~DeleteCommand() {
    if (!items_.isEmpty() && (*items_.begin())->scene() == nullptr) {
        qDeleteAll(items_);
    }
}

void DeleteCommand::execute() {
    for(const auto& i : items_){
        if (auto* item = dynamic_cast<BaseItem*>(i)) {
            if (item->kind() == BaseItem::Kind::Node) {
                auto* node = dynamic_cast<RectItem*>(item);
                if (node) {
                    for (auto* line : node->lines()) {
                        scene_->removeItem(line);
                    }
                }
            }else if (item->kind() == BaseItem::Kind::Edge) {
                auto* line = dynamic_cast<LineItem*>(item);
                if (line) {
                    auto* node = line->startItem();
                    if (node) {
                       node->removeLine(line);
                    }
                    node = line->endItem();
                    if (node) {
                        node->removeLine(line);
                    }
                }
            }
        }
        scene_->removeItem(i);
    }
    // for (auto* item : items_) ;
    adjustContainersForItems(items_);
    adjustContainersForParents(parents_);
}

void DeleteCommand::unExecute() {
    for (auto* i : items_) {
        auto itParent = parents_.find(i);
        if (itParent != parents_.end() && itParent.value()) {
            i->setParentItem(itParent.value());
        }
        auto itPos = positions_.find(i);
        if (itPos != positions_.end()) {
            i->setPos(itPos.value());
        }
    }

    for(const auto& i : items_){
        if (auto* item = dynamic_cast<BaseItem*>(i)) {
            if (item->kind() == BaseItem::Kind::Node) {
                auto* node = dynamic_cast<RectItem*>(item);
                if (node) {
                    for (auto* line : node->lines()) {
                        scene_->addItem(line);
                    }
                }
            }else if (item->kind() == BaseItem::Kind::Edge) {
                auto* line = dynamic_cast<LineItem*>(item);
                if (line) {
                    auto* node = line->startItem();
                    if (node) {
                       node->addLine(line);
                    }
                    node = line->endItem();
                    if (node) {
                        node->addLine(line);
                    }
                }
            }
        }
        if (!i->parentItem()) {
            scene_->addItem(i);
        }
    }
    // for (auto* item : items_) scene_->addItem(item);
    adjustContainersForItems(items_);
    adjustContainersForParents(parents_);
}

SetPropertyCommand::SetPropertyCommand(RectItem* item, const QString& keyPath, const QVariant& oldValue, const QVariant& newValue, QUndoCommand* parent)
    : BaseCommand("Set Property", parent), item_(item), keyPath_(keyPath), oldValue_(oldValue), newValue_(newValue) {}

void SetPropertyCommand::execute() {
    if (!item_) return;
    item_->setPropByKeyPath(keyPath_, newValue_);
    item_->update();
}

void SetPropertyCommand::unExecute() {
    if (!item_) return;
    item_->setPropByKeyPath(keyPath_, oldValue_);
    item_->update();
}

DagConfigCommand::DagConfigCommand(QString* nameRef, QString* failPolicyRef, int* maxParallelRef,
                                   const QString& oldName, const QString& newName,
                                   const QString& oldFail, const QString& newFail,
                                   int oldMax, int newMax,
                                   QUndoCommand* parent)
    : BaseCommand("Edit DAG Config", parent),
      nameRef_(nameRef), failPolicyRef_(failPolicyRef), maxParallelRef_(maxParallelRef),
      oldName_(oldName), newName_(newName),
      oldFail_(oldFail), newFail_(newFail),
      oldMax_(oldMax), newMax_(newMax) {}

void DagConfigCommand::execute() {
    if (nameRef_) *nameRef_ = newName_;
    if (failPolicyRef_) *failPolicyRef_ = newFail_;
    if (maxParallelRef_) *maxParallelRef_ = newMax_;
}

void DagConfigCommand::unExecute() {
    if (nameRef_) *nameRef_ = oldName_;
    if (failPolicyRef_) *failPolicyRef_ = oldFail_;
    if (maxParallelRef_) *maxParallelRef_ = oldMax_;
}
