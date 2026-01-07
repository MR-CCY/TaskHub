#pragma once
#include "task.h"

class CanvasScene;

class LayoutTask : public Task {
public:
    explicit LayoutTask(CanvasScene* scene, QObject* parent = nullptr);

    bool dispatch(QEvent* e) override;
    void execute();

private:
    CanvasScene* scene_ = nullptr;
};
