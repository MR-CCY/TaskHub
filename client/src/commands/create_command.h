#pragma once
#include "command.h"
#include <QGraphicsItem>
#include <QGraphicsScene>

class CreateCommand : public BaseCommand {
public:
    CreateCommand(QGraphicsScene* scene, QGraphicsItem* item, QUndoCommand* parent = nullptr)
        : BaseCommand("Create Item", parent), scene_(scene), item_(item) 
    {
        // 第一次创建时，通常 item 还没有被加到 scene，或者已经被加了
        // 我们假设 Operator 传进来的是还没加到 scene 的 item，或者刚加进去
    }

    ~CreateCommand() {
        // 如果 Item 目前不在 Scene 里（被撤销状态），Command 析构时要负责删掉它
        // 注意：QUndoStack 清空时会析构 Command
        if (item_ && !item_->scene()) {
            delete item_;
        }
    }

    void execute() override {
        if (item_ && !item_->scene()) {
            scene_->addItem(item_);
        }
    }

    void unExecute() override {
        if (item_ && item_->scene()) {
            scene_->removeItem(item_);
            // 注意：removeItem 不会 delete，所有权回到 Command 手里
        }
    }

private:
    QGraphicsScene* scene_;
    QGraphicsItem* item_;
};
