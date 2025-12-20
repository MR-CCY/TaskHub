#include "workflow_canvas_view.h"
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QGestureEvent>
#include <QPinchGesture>
#include <QScrollBar>

#include "commands/undostack.h"

WorkflowCanvasView::WorkflowCanvasView(QWidget* parent)
    : QGraphicsView(parent)
{
    setRenderHint(QPainter::Antialiasing, true);
    setDragMode(QGraphicsView::RubberBandDrag); // Q1-2 框选会用到；现在也不冲突
    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);

    // 让滚轮缩放以鼠标位置为中心更自然
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);

    // 触控板/触屏手势入口（Q1-5 要用）
    viewport()->setAttribute(Qt::WA_AcceptTouchEvents, true);
    grabGesture(Qt::PinchGesture);
}

void WorkflowCanvasView::setUndoStack(UndoStack* undo) {
    undo_ = undo;
}

bool WorkflowCanvasView::event(QEvent* e) {
    // Pinch 手势（Q1-5：这里先提供最小可用缩放）
    if (e->type() == QEvent::Gesture) {
        auto* ge = static_cast<QGestureEvent*>(e);
        if (auto* pinch = static_cast<QPinchGesture*>(ge->gesture(Qt::PinchGesture))) {
            // scaleFactor 是相对倍率
            const qreal factor = pinch->scaleFactor();
            if (factor > 0.0) {
                scale(factor, factor);
            }
            ge->accept(pinch);
            return true;
        }
    }
    return QGraphicsView::event(e);
}

void WorkflowCanvasView::wheelEvent(QWheelEvent* e) {
    if (!wheelZoomEnabled_) {
        QGraphicsView::wheelEvent(e);
        return;
    }

    // 兼容 Qt5/Qt6：角度增量
    const QPoint angleDelta = e->angleDelta();
    if (angleDelta.isNull()) {
        QGraphicsView::wheelEvent(e);
        return;
    }

    // 一般每 120 是一档
    const int steps = angleDelta.y() / 120;
    if (steps != 0) {
        zoomBySteps(steps, e->position().toPoint());
        e->accept();
        return;
    }

    QGraphicsView::wheelEvent(e);
}

void WorkflowCanvasView::zoomBySteps(int steps, const QPoint& /*viewportPos*/) {
    // 每档倍率（你可后续调参）
    const qreal base = 1.15;
    qreal factor = 1.0;
    if (steps > 0) {
        for (int i = 0; i < steps; ++i) factor *= base;
    } else {
        for (int i = 0; i < -steps; ++i) factor /= base;
    }
    scale(factor, factor);
}

void WorkflowCanvasView::mousePressEvent(QMouseEvent* e) {
    emit rawMousePressed(e);

    // 中键平移（不影响 item 的鼠标事件：我们用 view 的拖拽，不抢左键）
    if (middlePanEnabled_ && e->button() == Qt::MiddleButton) {
        startMiddlePan(e->pos());
        e->accept();
        return;
    }

    QGraphicsView::mousePressEvent(e);
}

void WorkflowCanvasView::mouseMoveEvent(QMouseEvent* e) {
    emit rawMouseMoved(e);

    if (middlePanEnabled_ && middlePanning_) {
        updateMiddlePan(e->pos());
        e->accept();
        return;
    }

    QGraphicsView::mouseMoveEvent(e);
}

void WorkflowCanvasView::mouseReleaseEvent(QMouseEvent* e) {
    emit rawMouseReleased(e);

    if (middlePanEnabled_ && e->button() == Qt::MiddleButton && middlePanning_) {
        endMiddlePan();
        e->accept();
        return;
    }

    QGraphicsView::mouseReleaseEvent(e);
}

void WorkflowCanvasView::keyPressEvent(QKeyEvent* e) {
    emit rawKeyPressed(e);

    // Undo/Redo 快捷键（Q1-1 就能用）
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

    QGraphicsView::keyPressEvent(e);
}

void WorkflowCanvasView::startMiddlePan(const QPoint& pos) {
    middlePanning_ = true;
    lastPanPos_ = pos;
    setCursor(Qt::ClosedHandCursor);
}

void WorkflowCanvasView::updateMiddlePan(const QPoint& pos) {
    const QPoint delta = pos - lastPanPos_;
    lastPanPos_ = pos;

    // 通过滚动条平移
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
}

void WorkflowCanvasView::endMiddlePan() {
    middlePanning_ = false;
    unsetCursor();
}

