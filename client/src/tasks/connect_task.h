// connect_task.h
#pragma once
#include "task.h"
#include <QGraphicsLineItem>

class RectItem;

class ConnectTask : public Task {
public:
    explicit ConnectTask(QObject* parent = nullptr);
    ~ConnectTask() override;
    
    // 优先级高(200)，盖在 SelectTask 上
    bool dispatch(QEvent* e) override;
    bool handleBaseKeyEvent(QEvent* e) override;
    
private:
    void cancel(); // 取消连线状态
    void initFromSelection(); // 根据当前选中项确定起点并清理选择
    
    RectItem* startItem_ = nullptr;
    QGraphicsLineItem* dragLine_ = nullptr; // 临时的视觉虚线
};
