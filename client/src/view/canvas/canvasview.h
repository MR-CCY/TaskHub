#pragma once
#include <QGraphicsView>
#include <QPointer>
#include <QPoint>

#include "commands/undostack.h"

class TaskManager;
class StraightLineTask;
class LineItem;
/**
 * CanvasView
 *
 * STOC 架构中的 View：
 * - 唯一输入入口
 * - 事件转发给 TaskManager
 * - 视口行为（缩放/平移）
 * - 全局快捷键（Undo/Redo）
 */
class CanvasView : public QGraphicsView {
    Q_OBJECT
public:
    explicit CanvasView(QWidget* parent = nullptr);

    void setUndoStack(UndoStack* undo);
    void setTaskManager(TaskManager* mgr);
    // 视口行为开关
    void setWheelZoomEnabled(bool on)   { wheelZoomEnabled_ = on; }
    void setMiddlePanEnabled(bool on)   { middlePanEnabled_ = on; }
    UndoStack* undoStack() const { return undo_; }
    void zoomBySteps(int steps);
protected:
    // ✅ 唯一事件入口（强烈推荐 viewportEvent）
    bool viewportEvent(QEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    // ===== STOC：事件 → Task 栈 =====
    bool dispatchEventToTasks(QEvent* e);
    bool handleLineDoubleClick(QEvent* e);

    // ===== View 行为（不入 Undo / 不产生命令）=====
    void handleGestureEvent(QEvent* e);
    void handleWheelZoom(QWheelEvent* e);

    void startMiddlePan(const QPoint& pos);
    void updateMiddlePan(const QPoint& pos);
    void endMiddlePan();

private:
    QPointer<UndoStack> undo_;
    TaskManager* taskManager_ = nullptr;   // 重命名变量

    bool wheelZoomEnabled_  = true;
    bool middlePanEnabled_ = true;

    bool middlePanning_ = false;
    QPoint lastPanPos_;
    
    // For smooth trackpad zooming
    qreal wheelDeltaAccumulator_ = 0.0;
};
