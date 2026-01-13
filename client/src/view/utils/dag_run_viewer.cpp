#include "dag_run_viewer.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonDocument>
#include <QMessageBox>
#include <QGraphicsView>
#include <QVBoxLayout>

#include "view/utils/dag_loader.h"
#include "Item/rect_item.h"
#include "Item/line_item.h"
#include "Item/line_item_factory.h"
#include "Item/node_item_factory.h"
#include "Item/node_type.h"
#include "view/canvas/canvasscene.h"
#include "commands/undostack.h"
#include "view/canvas/canvasview.h"
#include <QDebug>

DagRunViewerBench::DagRunViewerBench(QWidget* parent)
    : CanvasBench(parent)
{
    // 添加简单布局让view充满空间
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(view());
    setLayout(layout);
    
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

QStringList DagRunViewerBench::getDagNodes() const
{
    QStringList list;
    for (auto it = idMap_.begin(); it != idMap_.end(); ++it) {
        RectItem* node = it.value();
        if (!node) continue;
        // Check exec_type
        QString execType = node->prop("exec_type").toString();
        if (execType.compare("Dag", Qt::CaseInsensitive) == 0) {
            list.append(it.key());
        }
    }
    return list;
}
