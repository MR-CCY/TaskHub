#include "dag_run_viewer.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonDocument>
#include <QMessageBox>
#include <QGraphicsView>

#include "view/dag_loader.h"
#include "Item/rect_item.h"
#include "Item/line_item.h"
#include "Item/line_item_factory.h"
#include "Item/node_item_factory.h"
#include "Item/node_type.h"
#include "view/canvasscene.h"
#include "commands/undostack.h"
#include "view/canvasview.h"
#include <QDebug>

DagRunViewerBench::DagRunViewerBench(QWidget* parent)
    : CanvasBench(parent)
{
    if (view()) {
        view()->setInteractive(false);           // 只读查看
        view()->setDragMode(QGraphicsView::ScrollHandDrag);
        view()->setWheelZoomEnabled(true);
        view()->setMiddlePanEnabled(true);
    }
}

bool DagRunViewerBench::loadDagJson(const QJsonObject& obj)
{
    if (!scene() || !undoStack()) return false;
    qDebug() << obj;

    QJsonArray tasks;
    if (obj.contains("tasks") && obj.value("tasks").isArray()) {
        tasks = obj.value("tasks").toArray();
    } else if (obj.contains("task") && obj.value("task").isObject()) {
        tasks.append(obj.value("task").toObject());
    } else if (obj.contains("id")) {
        tasks.append(obj);
    } else {
        return false;
    }
    qDebug() << tasks;
    scene()->clear();
    idMap_.clear();

    QHash<QString, RectItem*> dummyPreExisting;
    // outCreatedNodes will be our idMap_ but we probably want to just use idMap_ directly if possible?
    // idMap_ is member, check type. QHash<QString, RectItem*>.
    // DagLoader outCreatedNodes is QHash<QString, RectItem*>&.
    // So we can pass idMap_!
    DagLoader::loadLevel(tasks, scene(), undoStack(), nullptr, dummyPreExisting, idMap_);

    layoutDag();
    return true;
}

void DagRunViewerBench::setNodeStatus(const QString& nodeId, const QColor& color)
{
    if (auto* n = idMap_.value(nodeId, nullptr)) {
        n->setStatusOverlay(color);
    }
}

void DagRunViewerBench::setNodeStatusLabel(const QString& nodeId, const QString& label)
{
    if (auto* n = idMap_.value(nodeId, nullptr)) {
        n->setStatusLabel(label);
    }
}

void DagRunViewerBench::selectNode(const QString& nodeId)
{
    if (auto* n = idMap_.value(nodeId, nullptr)) {
        scene()->clearSelection();
        n->setSelected(true);
        emit nodeSelected(nodeId);
    }
}

QStringList DagRunViewerBench::getRemoteNodes() const
{
    QStringList list;
    for (auto it = idMap_.begin(); it != idMap_.end(); ++it) {
        RectItem* node = it.value();
        if (!node) continue;
        // Check exec_type
        QString execType = node->prop("exec_type").toString();
        if (execType.compare("Remote", Qt::CaseInsensitive) == 0) {
            list.append(it.key());
        }
    }
    return list;
}
