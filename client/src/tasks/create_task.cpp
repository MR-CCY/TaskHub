// create_task.cpp
#include "create_task.h"
#include "view/canvas/canvasview.h"
#include "view/canvas/canvasscene.h"
#include "Item/rect_item.h"
#include "Item/node_item_factory.h"
#include "Item/container_rect_item.h"
#include "operator/create_rect_operator.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
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

QJsonObject parseJsonObject(const QJsonValue& value) {
    if (value.isObject()) return value.toObject();
    if (value.isString()) {
        QJsonParseError pe;
        const auto doc = QJsonDocument::fromJson(value.toString().toUtf8(), &pe);
        if (pe.error == QJsonParseError::NoError) {
            if (doc.isObject()) return doc.object();
            if (doc.isArray()) {
                QJsonObject obj;
                obj.insert("params", doc.array());
                return obj;
            }
        }
    }
    return QJsonObject();
}
}

CreateTask::CreateTask(QObject* parent) 
    : Task(200, parent) // Level 100, 假设比 SelectTask 高
{}

CreateTask::CreateTask(NodeType type, const QString& templateId, const QJsonObject& taskTemplate, QObject* parent)
    : Task(200, parent),
      useFactory_(true),
      nodeType_(type),
      templateId_(templateId.trimmed()),
      taskTemplate_(taskTemplate)
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
            } else if (useFactory_ && !taskTemplate_.isEmpty()
                       && (nodeType_ == NodeType::Shell || nodeType_ == NodeType::Http || nodeType_ == NodeType::Local)) {
                const QString execCommand = taskTemplate_.value("exec_command").toString();
                if (!execCommand.isEmpty()) {
                    item->setProp("exec_command", execCommand);
                }

                QJsonObject execParamsObj = parseJsonObject(taskTemplate_.value("exec_params"));
                if (execParamsObj.isEmpty() && taskTemplate_.value("exec_params").isObject()) {
                    execParamsObj = taskTemplate_.value("exec_params").toObject();
                }
                if (!execParamsObj.isEmpty()) {
                    if (nodeType_ == NodeType::Shell && !execParamsObj.contains("cmd") && !execCommand.isEmpty()) {
                        execParamsObj.insert("cmd", execCommand);
                    }
                    item->setProp("exec_params", execParamsObj.toVariantMap());
                } else if (nodeType_ == NodeType::Shell && !execCommand.isEmpty()) {
                    item->setProp("exec_params.cmd", execCommand);
                }

                QJsonObject schemaObj = parseJsonObject(taskTemplate_.value("schema"));
                if (schemaObj.isEmpty()) {
                    schemaObj = parseJsonObject(taskTemplate_.value("schema_json"));
                }
                if (!schemaObj.isEmpty()) {
                    item->setProp("schema_json", schemaObj);
                }
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
