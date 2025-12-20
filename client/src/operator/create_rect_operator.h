#pragma once
#include "base_operator.h"
#include "base_item.h"

class CreateRectOperator : public BaseOperator {
public:
    CreateRectOperator(CanvasView* view, BaseItem* item)
        : BaseOperator(view), item_(item) {}

protected:
    bool innerDo(bool canUndo) override {
        // 调用 Item 的工厂方法来生成 Command
        // 这就是 STOC 的核心：Operator 不直接 new Command
        item_->execCreateCmd(canUndo); 
        return true;
    }

private:
    BaseItem* item_;
};