#pragma once
#include "task.h"
#include <QGraphicsItem>
#include <QPointF>

class MoveTask : public Task {
    Q_OBJECT
public:
    // Level 10: 比如 SelectTask(20) 轻，所以可以压在它上面
    explicit MoveTask(QGraphicsItem* target, QPointF startScenePos, QObject* parent = nullptr);
    ~MoveTask();

    bool dispatch(QEvent* e) override;

private:
    QGraphicsItem* target_ = nullptr;
    QPointF startPos_;        // RectItem 的初始位置
    QPointF clickOffset_;     // 鼠标点击位置相对于 RectItem origin 的偏移
    QPointF startControlOff_; // LineItem 的初始控制点偏移
    bool hasMoved_ = false;   // 标记是否真的动过
};