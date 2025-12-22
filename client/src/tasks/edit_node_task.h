#pragma once
#include "task.h"

class CanvasScene;
class UndoStack;
class CanvasView;
class RectItem;

// 节点属性编辑任务（一次性弹窗），Level 50，提交使用宏包裹
class EditNodeTask : public Task {
public:
    EditNodeTask(CanvasScene* scene, UndoStack* undo, CanvasView* view, QWidget* parentWidget = nullptr, QObject* parent = nullptr);
    bool dispatch(QEvent* e) override;
    void execute();

private:
    CanvasScene* scene_;
    UndoStack* undo_;
    CanvasView* view_;
    QWidget* parentWidget_;
    
};
