#pragma once
#include "task.h"

// 缩放任务：处理滚轮/触控缩放，Level 高于 SelectTask
class ZoomTask : public Task {
    Q_OBJECT
public:
    explicit ZoomTask(QObject* parent = nullptr);
    bool dispatch(QEvent* e) override;
private:
    qreal wheelAccum_ = 0.0; // 累积触摸板细粒度滚动
};
