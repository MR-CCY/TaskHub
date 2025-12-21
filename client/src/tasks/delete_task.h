#pragma once
#include "task.h"

class CanvasScene;
class UndoStack;

// 一次性删除任务：用当前 selection 创建 DeleteCommand，Level 200
class DeleteTask : public Task {
public:
    DeleteTask(CanvasScene* scene, UndoStack* undo, QObject* parent = nullptr);
    bool dispatch(QEvent* e) override;
    void execute();

private:
    CanvasScene* scene_ = nullptr;
    UndoStack* undo_ = nullptr;
};
