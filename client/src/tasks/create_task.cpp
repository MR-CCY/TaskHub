// create_task.cpp
#include "create_task.h"
#include "view/canvas/canvasview.h"
#include "view/canvas/canvasscene.h"
#include "Item/rect_item.h"
#include "Item/node_item_factory.h"
#include "Item/container_rect_item.h"
#include "operator/create_rect_operator.h"
#include <QMouseEvent>
#include <QDebug>

namespace {
ContainerRectItem* findContainerAt(QGraphicsScene* scene, const QPointF& scenePos) {
    if (!scene) return nullptr;
    const auto items = scene->items(scenePos);
    for (auto* item : items) {
        if (auto* container = dynamic_cast<ContainerRectItem*>(item)) {
            const QRectF inner = container->mapRectToScene(container->contentRect());
            if (inner.contains(scenePos)) {
                return container;
            }
        }
    }
    return nullptr;
}
}

CreateTask::CreateTask(QObject* parent) 
    : Task(200, parent) // Level 100, 假设比 SelectTask 高
{}

CreateTask::CreateTask(NodeType type, const QString& templateId, QObject* parent)
    : Task(200, parent),
      useFactory_(true),
      nodeType_(type),
      templateId_(templateId.trimmed())
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
            ContainerRectItem* container = findContainerAt(scene, pos);

            RectItem* item = nullptr;
            if (useFactory_) {
                item = NodeItemFactory::createNode(nodeType_, QRectF(0, 0, 150, 150), container);
            } else {
                item = new RectItem(QRectF(0, 0, 150, 150), container);
            }
            if (!item) {
                qWarning() << "CreateTask failed to create item";
                removeSelf();
                return true;
            }
            if (useFactory_ && nodeType_ == NodeType::Template && !templateId_.isEmpty()) {
                item->setProp("exec_params.template_id", templateId_);
            }

            if (container) {
                item->setPos(container->mapFromScene(pos));
            } else {
                item->setPos(pos);
            }
            
            // 3. 注入上下文 (Crucial Step)
            item->attachContext(scene, nullptr, undo);

            // 4. 创建 Operator (Service) 并执行
            CreateRectOperator op(v, item);
            op.doOperator(true);
            if (container) {
                container->adjustToChildren();
            }
            
            // 5. Select the newly created item
            scene->clearSelection();
            item->setSelected(true);

            // 5. 任务完成，退出自己（一次性任务）
            removeSelf();
            
            return true; // 事件已处理
        }
    }
    return false;
}
