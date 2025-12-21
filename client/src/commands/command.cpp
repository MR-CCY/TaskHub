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
