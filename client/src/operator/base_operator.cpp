// base_operator.cpp
#include "base_operator.h"
#include "commands/undostack.h" // 假设 View 有 getUndoStack()

BaseOperator::BaseOperator(CanvasView* view, BaseOperator* parent)
    : view_(view), parent_(parent) {}

BaseOperator::~BaseOperator() {
    qDeleteAll(children_);
}

void BaseOperator::addChild(BaseOperator* op) {
    children_.append(op);
}

bool BaseOperator::doOperator(bool canUndo) {
    // 1. 获取 UndoStack (假设你在 CanvasView 加了 undoStack() 访问器)
    // 这里需要你在 CanvasView.h 增加 undoStack() 的 public 接口
    UndoStack* stack = view_->undoStack(); 
    // 暂时用伪代码，你需要确保 CanvasView 能返回 UndoStack*
    
    // 如果是根 Operator，开启宏
    if (canUndo && !parent_ && stack) stack->beginMacro("User Action");

    // 2. 执行子 Operator
    for (auto* child : children_) {
        child->doOperator(canUndo);
    }

    // 3. 执行自己
    bool res = innerDo(canUndo);

    // 结束宏
    if (canUndo && !parent_ && stack) stack->endMacro();
    
    return res;
}