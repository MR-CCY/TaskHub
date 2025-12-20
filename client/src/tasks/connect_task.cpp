// connect_task.cpp
#include "connect_task.h"

#include <QGraphicsScene>
#include <QMouseEvent>

#include "Item/line_item.h"
#include "Item/rect_item.h"
#include "commands/command.h"
#include "view/canvasview.h"
#include "view/canvasscene.h"
#include <QMouseEvent>
#include <QGraphicsScene>

ConnectTask::ConnectTask(QObject* parent) 
    : Task(200, parent) // Level 200, 阻止底层的移动操作
{}

ConnectTask::~ConnectTask() {
    if (dragLine_) delete dragLine_;
}

bool ConnectTask::dispatch(QEvent* e) {
    // 处理右键取消
    if (e->type() == QEvent::MouseButtonPress) {
        if (static_cast<QMouseEvent*>(e)->button() == Qt::RightButton) {
            cancel();
            removeSelf(); // 退出任务
            return true;
        }
    }

    if (e->type() == QEvent::MouseButtonPress && static_cast<QMouseEvent*>(e)->button() == Qt::LeftButton) {
        auto* me = static_cast<QMouseEvent*>(e);
        QPointF scenePos = view()->mapToScene(me->pos());
        RectItem* rectItem = nullptr;
        QList<QGraphicsItem*> items = view()->scene()->items(scenePos); // 获取该点所有 items
        
        for (auto* it : items) {
            // 排除掉那个临时的虚线 (dragLine_)
            if (it == dragLine_) continue;
            
            // 尝试转换
            rectItem = dynamic_cast<RectItem*>(it);
            if (rectItem) {
                break; // 找到了！跳出循环
            }
        }

        if (!startItem_) {
            // 第一步：选中起点
            if (rectItem) {
                startItem_ = rectItem;
                // 创建临时虚线
                dragLine_ = new QGraphicsLineItem(QLineF(scenePos, scenePos));
                QPen pen(Qt::black, 2, Qt::DashLine);
                dragLine_->setPen(pen);
                view()->scene()->addItem(dragLine_);
            }
        } else {
            // 第二步：选中终点并连线
            if (rectItem && rectItem != startItem_) {
                bool alreadyConnected = false;
                // 遍历起点连接的所有线
                // 假设 RectItem 有 lines() 返回 QSet<LineItem*> 或 QList
                for (auto* existingLine : startItem_->lines()) {
                    // 如果这条线的另一端是我们要连的目标，说明已经连过了
                    if (existingLine->startItem() == rectItem || existingLine->endItem() == rectItem) {
                        alreadyConnected = true;
                        break;
                    }
                }
            
                if (alreadyConnected) {
                    qDebug() << "Connection already exists!";
                    // 可以选择：什么都不做，或者取消当前操作
                    // 这里我们选择忽略这次点击，用户可以去点别的，或者右键取消
                    return true; 
                }
                // ========================
            
                // 创建真实的连接线
                auto* line = new LineItem(startItem_, rectItem);
                // 注入上下文并执行命令
                line->attachContext(dynamic_cast<CanvasScene*>(view()->scene()), nullptr, view()->undoStack());
                line->execCreateCmd(true); // 存入 UndoStack

                // 连线完成，清理临时状态，退出任务
                cancel(); 
                removeSelf();
            }
        }
        return true;
    }
    
    if (e->type() == QEvent::MouseMove && startItem_ && dragLine_) {
        // 更新虚线位置
        auto* me = static_cast<QMouseEvent*>(e);
        QPointF scenePos = view()->mapToScene(me->pos());
        QPointF startPos = startItem_->sceneBoundingRect().center();
        dragLine_->setLine(QLineF(startPos, scenePos));
        return true;
    }

    return false;
}

void ConnectTask::cancel() {
    if (dragLine_) {
        view()->scene()->removeItem(dragLine_);
        delete dragLine_;
        dragLine_ = nullptr;
    }
    startItem_ = nullptr;
}
