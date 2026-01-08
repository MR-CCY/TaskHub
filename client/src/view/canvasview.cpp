#include "canvasview.h"

#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QGestureEvent>
#include <QPinchGesture>
#include <QScrollBar>

#include "commands/undostack.h"
#include "tasks/task_manager.h"
#include "tasks/straight_line_task.h"
#include "Item/line_item.h"


CanvasView::CanvasView(QWidget* parent)
    : QGraphicsView(parent)
{
    setRenderHint(QPainter::Antialiasing, true);

    // 使用自定义的 SelectTask / MoveTask 处理交互，禁用默认拖拽模式以防冲突
    setDragMode(QGraphicsView::NoDrag);
    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);

    // 缩放以鼠标为中心
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);

    // 触控/手势入口（Q1-5）
    viewport()->setAttribute(Qt::WA_AcceptTouchEvents, true);
    grabGesture(Qt::PinchGesture);
}

void CanvasView::setUndoStack(UndoStack* undo) {
    undo_ = undo;
}

void CanvasView::setTaskManager(TaskManager* mgr)
{
    taskManager_ = mgr;
}
/**
   * 核心入口：
   * 所有鼠标/触控事件都会从这里进来
   */
bool CanvasView::viewportEvent(QEvent* e) {
    // 优先处理线条双击直线化
    if (e->type() == QEvent::MouseButtonDblClick && handleLineDoubleClick(e)) {
        return true;
    }

    // 1️⃣ 先给 Task 栈（STOC：交互解释权在 Task）
    if (dispatchEventToTasks(e)) {
        return true;   // 被某个 Task accept，终止传播
    }

    // 2️⃣ View 自己处理的行为（不入 Command）
    switch (e->type()) {
    case QEvent::Gesture:
        handleGestureEvent(e);
        return true;

    case QEvent::Wheel:
        if (wheelZoomEnabled_) {
            handleWheelZoom(static_cast<QWheelEvent*>(e));
            return true;
        }
        break;

    case QEvent::MouseButtonPress: {
        auto* me = static_cast<QMouseEvent*>(e);
        if (middlePanEnabled_ && me->button() == Qt::MiddleButton) {
            startMiddlePan(me->pos());
            e->accept();
            return true;
        }
        break;
    }

    case QEvent::MouseMove: {
        auto* me = static_cast<QMouseEvent*>(e);
        if (middlePanEnabled_ && middlePanning_) {
            updateMiddlePan(me->pos());
            e->accept();
            return true;
        }
        break;
    }

    case QEvent::MouseButtonRelease: {
        auto* me = static_cast<QMouseEvent*>(e);
        if (middlePanEnabled_ &&
            middlePanning_ &&
            me->button() == Qt::MiddleButton) {
            endMiddlePan();
            e->accept();
            return true;
        }
        break;
    }

    default:
        break;
    }

    return QGraphicsView::viewportEvent(e);
}

/**
 * 键盘事件：
 * - Ctrl+Z / Ctrl+Y 是 View 的职责
 * - 其余交给 Task 栈
 */
void CanvasView::keyPressEvent(QKeyEvent* e) {
    if (undo_) {
        if (e->matches(QKeySequence::Undo)) {
            undo_->undo();
            e->accept();
            return;
        }
        if (e->matches(QKeySequence::Redo)) {
            undo_->redo();
            e->accept();
            return;
        }
    }

    if (dispatchEventToTasks(e)) {
        return;
    }

    QGraphicsView::keyPressEvent(e);
}

/**
 * ===== STOC：事件 → Task 栈 =====
 */
bool CanvasView::dispatchEventToTasks(QEvent* e) {
    if (!taskManager_) return false;
    // 语义：top task → next → …，谁 accept 就停
    return taskManager_->dispatch(e);
}

bool CanvasView::handleLineDoubleClick(QEvent* e) {
    if (!taskManager_ || e->type() != QEvent::MouseButtonDblClick) return false;
    auto* me = static_cast<QMouseEvent*>(e);
    if (me->button() != Qt::LeftButton) return false;

    const QPointF scenePos = mapToScene(me->pos());
    const auto itemsAtPos = scene()->items(scenePos);
    for (auto* it : itemsAtPos) {
        if (auto* line = dynamic_cast<LineItem*>(it)) {
            auto* task = new StraightLineTask(line, this);
            taskManager_->push(task);
            task->straighten();
            e->accept();
            return true;
        }
    }
    return false;
}

/**
 * ===== 手势（Pinch）=====
 */
void CanvasView::handleGestureEvent(QEvent* e) {
    auto* ge = static_cast<QGestureEvent*>(e);
    if (auto* pinch =
            static_cast<QPinchGesture*>(ge->gesture(Qt::PinchGesture))) {

        const qreal factor = pinch->scaleFactor();
        if (factor > 0.0) {
            scale(factor, factor);
        }
        ge->accept(pinch);
    }
}

/**
 * ===== 滚轮缩放 =====
 */
void CanvasView::handleWheelZoom(QWheelEvent* e) {
    const QPoint delta = e->angleDelta();
    if (delta.isNull()) return;

    const int steps = delta.y() / 120;
    if (steps != 0) {
        zoomBySteps(steps);
        e->accept();
    }
}

void CanvasView::zoomBySteps(int steps) {
    constexpr qreal base = 1.15;
    qreal factor = 1.0;

    if (steps > 0) {
        for (int i = 0; i < steps; ++i) factor *= base;
    } else {
        for (int i = 0; i < -steps; ++i) factor /= base;
    }
    scale(factor, factor);
}

/**
 * ===== 中键平移 =====
 */
void CanvasView::startMiddlePan(const QPoint& pos) {
    middlePanning_ = true;
    lastPanPos_ = pos;
    setCursor(Qt::ClosedHandCursor);
}

void CanvasView::updateMiddlePan(const QPoint& pos) {
    const QPoint delta = pos - lastPanPos_;
    lastPanPos_ = pos;

    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
}

void CanvasView::endMiddlePan() {
    middlePanning_ = false;
    unsetCursor();
}
