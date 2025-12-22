// create_task.cpp
#include "create_task.h"
#include "view/canvasview.h"
#include "view/canvasscene.h"
#include "Item/rect_item.h"
#include "Item/node_item_factory.h"
#include "operator/create_rect_operator.h"
#include <QMouseEvent>
#include <QDebug>

CreateTask::CreateTask(QObject* parent) 
    : Task(200, parent) // Level 100, 假设比 SelectTask 高
{}

CreateTask::CreateTask(NodeType type, QObject* parent)
    : Task(200, parent), useFactory_(true), nodeType_(type)
{}

bool CreateTask::dispatch(QEvent* e) {
    if (e->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(e);
        if (me->button() == Qt::LeftButton) {
            
            // 1. 获取 View, Scene, UndoStack
            // 这里假设 Task 有 view() 方法（base_task 里有 setView）
            auto* v = view(); 
            auto* scene = v->scene();
            // 假设 CanvasView 有 undoStack() 接口
            auto* undo = v->undoStack(); 

            // 2. 创建 Item (Model)
            auto pos = v->mapToScene(me->pos());
            RectItem* item = nullptr;
            if (useFactory_) {
                item = NodeItemFactory::createNode(nodeType_, QRectF(0, 0, 150, 150));
            } else {
                item = new RectItem(QRectF(0, 0, 150, 150));
            }
            if (!item) {
                qWarning() << "CreateTask failed to create item";
                removeSelf();
                return true;
            }
            item->setPos(pos);
            
            // 3. 注入上下文 (Crucial Step)
            item->attachContext(scene, nullptr, undo);

            // 4. 创建 Operator (Service) 并执行
            CreateRectOperator op(v, item);
            op.doOperator(true);

            // 5. 任务完成，退出自己（一次性任务）
            removeSelf();
            
            return true; // 事件已处理
        }
    }
    return false;
}
