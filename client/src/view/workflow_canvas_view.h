#pragma once
#include <QGraphicsView>
#include <QPointer>

class UndoStack;

// 你的 QGraphicsView 子类：负责视口交互 + 快捷键 + 手势入口
class WorkflowCanvasView : public QGraphicsView {
    Q_OBJECT
public:
    explicit WorkflowCanvasView(QWidget* parent=nullptr);

    void setUndoStack(UndoStack* undo);

    // 视口行为开关
    void setWheelZoomEnabled(bool on) { wheelZoomEnabled_ = on; }
    void setMiddlePanEnabled(bool on) { middlePanEnabled_ = on; }

signals:
    // Q1-2：把原始事件往外抛给 TaskManager（接口位）
    void rawMousePressed(QMouseEvent* e);
    void rawMouseMoved(QMouseEvent* e);
    void rawMouseReleased(QMouseEvent* e);
    void rawKeyPressed(QKeyEvent* e);

protected:
    void wheelEvent(QWheelEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

    bool event(QEvent* e) override; // 用于手势（pinch）入口

private:
    void zoomBySteps(int steps, const QPoint& viewportPos);
    void startMiddlePan(const QPoint& pos);
    void updateMiddlePan(const QPoint& pos);
    void endMiddlePan();

private:
    QPointer<UndoStack> undo_;

    bool wheelZoomEnabled_ = true;
    bool middlePanEnabled_ = true;

    bool middlePanning_ = false;
    QPoint lastPanPos_;
};