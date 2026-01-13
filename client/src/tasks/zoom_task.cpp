#include "zoom_task.h"

#include <QWheelEvent>
#include <QGestureEvent>
#include <QPinchGesture>
#include <QNativeGestureEvent>

#include "view/canvas/canvasview.h"

ZoomTask::ZoomTask(QObject* parent)
    : Task(15, parent) // 高于 SelectTask(10)
{}

bool ZoomTask::dispatch(QEvent* e) {
    if (!view()) return false;

    if (e->type() == QEvent::Wheel) {
        auto* we = static_cast<QWheelEvent*>(e);
        QPoint delta = we->angleDelta();
        if (delta.isNull()) {
            delta = we->pixelDelta(); // 触摸板细粒度
        }
        if (delta.isNull()) return false;

        wheelAccum_ += delta.y();
        int steps = static_cast<int>(wheelAccum_ / 120.0);
        wheelAccum_ -= steps * 120.0;
        if (steps == 0) {
            return false;
        }
        view()->zoomBySteps(steps);
        e->accept();
        return true;
    }

    if (e->type() == QEvent::Gesture) {
        auto* ge = static_cast<QGestureEvent*>(e);
        if (auto* pinch = static_cast<QPinchGesture*>(ge->gesture(Qt::PinchGesture))) {
            const qreal factor = pinch->scaleFactor();
            if (factor > 0.0) {
                view()->scale(factor, factor);
                ge->accept(pinch);
                return true;
            }
        }
    }

    if (e->type() == QEvent::NativeGesture) {
        // macOS 触摸板两指缩放会走 NativeGesture
        auto* nge = static_cast<QNativeGestureEvent*>(e);
        if (nge->gestureType() == Qt::ZoomNativeGesture) {
            const qreal delta = nge->value(); // 正负表示放大缩小
            const qreal factor = 1.0 + delta;
            if (factor > 0.0) {
                view()->scale(factor, factor);
                e->accept();
                return true;
            }
        }
    }

    return false;
}
