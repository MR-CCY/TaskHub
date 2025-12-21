#pragma once
#include "task.h"

class LineItem;

// 一次性任务：将弯线拉直（control offset 归零），Level 1
class StraightLineTask : public Task {
public:
    explicit StraightLineTask(LineItem* line, QObject* parent = nullptr);
    bool dispatch(QEvent* e) override;
    void straighten();

private:
    LineItem* line_ = nullptr;
};
