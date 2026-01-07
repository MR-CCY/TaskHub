#include "dag_run_viewer.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonDocument>
#include <QMessageBox>
#include <QGraphicsView>

#include "Item/rect_item.h"
#include "Item/line_item.h"
#include "Item/line_item_factory.h"
#include "Item/node_item_factory.h"
#include "Item/node_type.h"
#include "view/canvasscene.h"
#include "commands/undostack.h"
#include "view/canvasview.h"

namespace {
NodeType typeFromExec(const QString& execType) {
    const QString t = execType.toLower();
    if (t == "shell") return NodeType::Shell;
    if (t == "http" || t == "httpcall") return NodeType::Http;
    if (t == "remote") return NodeType::Remote;
    if (t == "dag") return NodeType::Dag;
    if (t == "template") return NodeType::Template;
    return NodeType::Local;
}

}

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

    // 支持三种格式：
    // 1) { "tasks": [ ... ] }
    // 2) { "task": { ... } }
    // 3) { "id": "...", ... }  // 单任务直接扁平在顶层
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

    scene()->clear();
    idMap_.clear();

    QList<RectItem*> nodes;
    for (const auto& jt : tasks) {
        if (!jt.isObject()) continue;
        const QJsonObject t = jt.toObject();
        const QString execType = t.value("exec_type").toString("Local");
        NodeType nt = typeFromExec(execType);
        RectItem* node = NodeItemFactory::createNode(nt, QRectF(0, 0, 140, 140));
        if (!node) continue;

        // props
        QVariantMap cfg;
        static const char* keys[] = {
            "id", "name", "exec_type", "exec_command", "exec_params",
            "timeout_ms", "retry_count", "retry_delay_ms", "retry_exp_backoff",
            "priority", "queue", "capture_output", "metadata"
        };
        for (auto key : keys) {
            const QString sk(key);
            if (!t.contains(sk)) continue;
            const auto val = t.value(sk);
            if (sk == "exec_params" || sk == "metadata") {
                cfg[sk] = val.toObject().toVariantMap();
            } else if (sk == "timeout_ms" || sk == "retry_delay_ms") {
                cfg[sk] = val.toVariant().toLongLong();
            } else if (sk == "retry_count" || sk == "priority") {
                cfg[sk] = val.toInt();
            } else if (sk == "retry_exp_backoff" || sk == "capture_output") {
                cfg[sk] = val.toBool();
            } else {
                cfg[sk] = val.toString();
            }
        }
        node->setTaskConfig(cfg);

        node->attachContext(scene(), nullptr, undoStack());
        node->execCreateCmd(true);
        nodes.append(node);
    }

    // map id -> node
    for (auto* n : nodes) {
        const QString id = n->prop("id").toString();
        if (!id.isEmpty()) idMap_[id] = n;
    }

    // 连线
    for (const auto& jt : tasks) {
        const QJsonObject t = jt.toObject();
        const QString id = t.value("id").toString();
        auto* target = idMap_.value(id, nullptr);
        if (!target) continue;
        const QJsonArray deps = t.value("deps").toArray();
        for (const auto& depVal : deps) {
            const QString depId = depVal.toString();
            auto* src = idMap_.value(depId, nullptr);
            if (!src || src == target) continue;
            QGraphicsItem* parent = nullptr;
            if (!LineItemFactory::canConnect(src, target, parent)) {
                continue;
            }
            auto* line = LineItemFactory::createLine(src, target, parent);
            if (!line) continue;
            line->attachContext(scene(), nullptr, undoStack());
            line->execCreateCmd(true);
        }
    }

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
