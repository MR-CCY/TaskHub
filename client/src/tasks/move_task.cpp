#include "move_task.h"
#include "view/canvasview.h"
#include "commands/command.h" // 假设你的 MoveRectCommand 在这里
#include "Item/rect_item.h"
#include "Item/line_item.h"
#include <QMouseEvent>
#include <QDebug>

MoveTask::MoveTask(QGraphicsItem* target, QPointF startScenePos, QObject* parent)
    : Task(10, parent), target_(target)
{
    // 初始化记录原始状态
    if (auto* rect = dynamic_cast<RectItem*>(target_)) {
        startPos_ = rect->pos();
    } 
    else if (auto* line = dynamic_cast<LineItem*>(target_)) {
        startControlOff_ = line->controlOffset();
    }
}

MoveTask::~MoveTask()
{
    if (view()) {
        view()->unsetCursor(); 
    }
}

bool MoveTask::dispatch(QEvent* e)
{
    // 1. 鼠标移动：实时更新图元位置 (View/Model 更新)
    if (e->type() == QEvent::MouseMove) {
        auto* me = static_cast<QMouseEvent*>(e);
        QPointF scenePos = view()->mapToScene(me->pos());
        
        hasMoved_ = true;
        // 1. 设置拖拽光标（只设置一次即可，或者每次设也无妨）
        if (!hasMoved_) {
            view()->setCursor(Qt::ClosedHandCursor);
            hasMoved_ = true;
        }

        if (auto* rect = dynamic_cast<RectItem*>(target_)) {
            // 移动 Rect：直接 setPos
            // 因为 RectItem 内部实现了 itemChange 通知 LineItem，所以线会自动跟随
            rect->setPos(scenePos - rect->boundingRect().center());
        } 
        else if (auto* line = dynamic_cast<LineItem*>(target_)) {
            // 移动 Line：调整贝塞尔控制点
            QPointF p1 = line->mapFromItem(line->startItem(), line->startItem()->boundingRect().center());
            QPointF p2 = line->mapFromItem(line->endItem(), line->endItem()->boundingRect().center());
            QPointF center = (p1 + p2) / 2;
            line->setControlOffset(scenePos - center);
        }
        return true;
    }

    // 2. 鼠标松开：提交命令并自杀 (Commit & Exit)
    if (e->type() == QEvent::MouseButtonRelease) {
        view()->unsetCursor();
        if (hasMoved_) {
            if (auto* rect = dynamic_cast<RectItem*>(target_)) {
                if (rect->pos() != startPos_) {
                    auto* cmd = new MoveRectCommand(rect, startPos_, rect->pos());
                    view()->undoStack()->push(cmd);
                }
            }
            else if (auto* line = dynamic_cast<LineItem*>(target_)) {
                if (line->controlOffset() != startControlOff_) {
                    auto* cmd = new AdjustLineCommand(line, startControlOff_, line->controlOffset());
                    view()->undoStack()->push(cmd);
                }
            }
        }
        
        // 任务结束，自行移除，控制权交还给下层的 SelectTask
        removeSelf();
        return true;
    }

    return false; // 其他事件忽略
}