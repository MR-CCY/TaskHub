#include "command.h"

#include "Item/line_item.h"
#include "Item/rect_item.h"

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

void MoveRectCommand::execute() { item_->setPos(newPos_); }
void MoveRectCommand::unExecute() { item_->setPos(oldPos_); }

AdjustLineCommand::AdjustLineCommand(LineItem* item, QPointF oldOff, QPointF newOff)
    : BaseCommand("Adjust Line"), item_(item), oldOff_(oldOff), newOff_(newOff) {}

void AdjustLineCommand::execute() { item_->setControlOffset(newOff_); }
void AdjustLineCommand::unExecute() { item_->setControlOffset(oldOff_); }

DeleteCommand::DeleteCommand(QGraphicsScene* scene, const QSet<QGraphicsItem*>& items)
    : BaseCommand("Delete"), scene_(scene), items_(items) {}

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
}

void DeleteCommand::unExecute() {
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
        scene_->addItem(i);
    }
    // for (auto* item : items_) scene_->addItem(item);
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
