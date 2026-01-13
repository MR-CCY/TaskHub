// connect_task.cpp
#include "connect_task.h"

#include <QGraphicsScene>
#include <QMouseEvent>
#include <QLineF>

#include "Item/line_item.h"
#include "Item/line_item_factory.h"
#include "Item/rect_item.h"
#include "commands/command.h"
#include "view/canvas/canvasview.h"
#include "view/canvas/canvasscene.h"

namespace {
QPointF projectToRectEdge(const QRectF& rect, const QPointF& target) {
    const QPointF center = rect.center();
    QLineF ray(center, target);
    const QPointF tl = rect.topLeft();
    const QPointF tr = rect.topRight();
    const QPointF br = rect.bottomRight();
    const QPointF bl = rect.bottomLeft();
    const QLineF edges[] = { QLineF(tl, tr), QLineF(tr, br), QLineF(br, bl), QLineF(bl, tl) };
    QPointF inter;
    for (const auto& edge : edges) {
        if (ray.intersects(edge, &inter) == QLineF::BoundedIntersection) {
            return inter;
        }
    }
    return target;
}

}

ConnectTask::ConnectTask(QObject* parent) 
    : Task(200, parent) // Level 200, 阻止底层的移动操作
{
    // 如果进入任务前已有选中节点，用第一个选中节点作为起点，并清空其余选择
    if (view() && view()->scene()) {
        const auto selected = view()->scene()->selectedItems();
        RectItem* firstRect = nullptr;
        for (auto* it : selected) {
            if (auto* r = dynamic_cast<RectItem*>(it)) {
                firstRect = r;
                break;
            }
        }
        if (firstRect) {
            startItem_ = firstRect;
            initFromSelection();
            for (auto* it : selected) {
                if (it != firstRect) it->setSelected(false);
            }
        }
    }
}

ConnectTask::~ConnectTask() {
    if (dragLine_) delete dragLine_;
}

bool ConnectTask::handleBaseKeyEvent(QEvent* e) {
    if (e->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(e);
        if (ke->key() == Qt::Key_Escape) {
            cancel(); // 清理临时虚线
        }
    }
    return Task::handleBaseKeyEvent(e);
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
                QGraphicsItem* parent = nullptr;
                if (!LineItemFactory::canConnect(startItem_, rectItem, parent)) {
                    qDebug() << "Connection blocked: cross-container nodes.";
                    return true;
                }
                auto* line = LineItemFactory::createLine(startItem_, rectItem, parent);
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
        QRectF startRect = startItem_->mapRectToScene(startItem_->boundingRect());
        QPointF startPos = projectToRectEdge(startRect, scenePos);
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

void ConnectTask::initFromSelection()
{
    if(dragLine_){
        delete dragLine_;
        dragLine_=nullptr;
    }
    QCursor *me = new QCursor;
    QPointF scenePos = view()->mapToScene(me->pos());
    dragLine_ = new QGraphicsLineItem(QLineF(scenePos, scenePos));
    QPen pen(Qt::black, 2, Qt::DashLine);
    dragLine_->setPen(pen);
    view()->scene()->addItem(dragLine_);
}
