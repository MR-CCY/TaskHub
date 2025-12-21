#pragma once
#include "task.h"

// 兜底任务 / 守护进程
// Level = 10000 (最重，永远在栈底)
class MainTask : public Task {
    Q_OBJECT
public:
    explicit MainTask(QObject* parent = nullptr);

    // 核心职责：如果发现自己裸露在栈顶，立刻复活 SelectTask
    bool dispatch(QEvent* e) override;
    bool handleBaseKeyEvent(QEvent* e) override { Q_UNUSED(e); return false; } // MainTask 不自销毁
};
