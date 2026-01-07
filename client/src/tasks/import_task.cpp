#include "import_task.h"

#include <QFileDialog>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>

#include "view/canvasscene.h"
#include "view/canvasview.h"
#include "commands/undostack.h"
#include "tasks/layout_task.h"
#include "Item/rect_item.h"
#include "Item/line_item.h"
#include "Item/line_item_factory.h"
#include "Item/node_item_factory.h"
#include "Item/node_type.h"
#include "layout_task.h"
#include "task_manager.h"
ImportTask::ImportTask(CanvasScene* scene, UndoStack* undo, CanvasView* view, QWidget* parentWidget, QObject* parent)
    : Task(200, parent), scene_(scene), undo_(undo), view_(view), parentWidget_(parentWidget) {}

bool ImportTask::dispatch(QEvent* e) {
    Q_UNUSED(e);
    return false;
}

void ImportTask::execute() {
    if (!scene_ || !undo_ || !view_) {
        removeSelf();
        return;
    }

    const QString path = QFileDialog::getOpenFileName(parentWidget_, QObject::tr("导入 JSON"), QString(), QObject::tr("JSON Files (*.json);;All Files (*)"));
    if (path.isEmpty()) {
        removeSelf();
        return;
    }

    QJsonObject root;
    if (!parseJson(path, root)) {
        QMessageBox::warning(parentWidget_, tr("导入失败"), tr("JSON 解析失败或格式非法"));
        removeSelf();
        return;
    }

    QJsonArray tasks;
    if (!validateTasks(root, tasks)) {
        QMessageBox::warning(parentWidget_, tr("导入失败"), tr("tasks 字段缺失或格式非法"));
        removeSelf();
        return;
    }

    // 现有节点映射
    QHash<QString, RectItem*> existing;
    for (auto* gitem : scene_->items()) {
        if (auto* r = dynamic_cast<RectItem*>(gitem)) {
            const QString id = r->prop("id").toString();
            if (!id.isEmpty()) {
                existing[id] = r;
            }
        }
    }

    // 处理冲突并映射 id -> RectItem
    QHash<QString, RectItem*> idToNode;
    if (!resolveConflicts(tasks, existing, idToNode)) {
        removeSelf();
        return;
    }

    // 创建缺失节点（宏包裹）
    undo_->beginMacro("Import DAG");
    for (const auto& jt : tasks) {
        const QJsonObject obj = jt.toObject();
        const QString id = obj.value("id").toString();
        if (idToNode.contains(id)) continue; // 已有或复用

        const QString execType = obj.value("exec_type").toString();
        NodeType nt = typeFromExec(execType);
        RectItem* node = NodeItemFactory::createNode(nt, QRectF(0, 0, 140, 140));
        if (!node) continue;

        // props 填充
        QVariantMap cfg;
        static const char* keys[] = {
            "id", "name", "exec_type", "exec_command", "exec_params",
            "timeout_ms", "retry_count", "retry_delay_ms", "retry_exp_backoff",
            "priority", "queue", "capture_output", "metadata"
        };
        for (auto key : keys) {
            const QString sk(key);
            if (!obj.contains(sk)) continue;
            if (sk == "exec_params" || sk == "metadata") {
                cfg[sk] = jsonObjectToStringMap(obj.value(sk).toObject());
            } else if (sk == "timeout_ms" || sk == "retry_delay_ms") {
                cfg[sk] = obj.value(sk).toVariant().toLongLong();
            } else if (sk == "retry_count" || sk == "priority") {
                cfg[sk] = obj.value(sk).toInt();
            } else if (sk == "retry_exp_backoff" || sk == "capture_output") {
                cfg[sk] = obj.value(sk).toBool();
            } else {
                cfg[sk] = obj.value(sk).toString();
            }
        }
        node->setTaskConfig(cfg);

        node->attachContext(scene_, nullptr, undo_);
        node->execCreateCmd(true);
        idToNode[id] = node;
    }

    // 创建连线
    for (const auto& jt : tasks) {
        const QJsonObject obj = jt.toObject();
        const QString id = obj.value("id").toString();
        auto* target = idToNode.value(id, nullptr);
        if (!target) continue;
        const QJsonArray deps = obj.value("deps").toArray();
        for (const auto& depVal : deps) {
            const QString depId = depVal.toString();
            auto* src = idToNode.value(depId, nullptr);
            if (!src || src == target) continue;
            QGraphicsItem* parent = nullptr;
            if (!LineItemFactory::canConnect(src, target, parent)) {
                continue;
            }
            auto* line = LineItemFactory::createLine(src, target, parent);
            if (!line) continue;
            line->attachContext(scene_, nullptr, undo_);
            line->execCreateCmd(true);
        }
    }

    undo_->endMacro();
    if (auto* mgr = manager()) {
        auto* layoutTask = new LayoutTask(scene_, mgr);
        mgr->push(layoutTask);
        layoutTask->execute();
    }
    removeSelf();
}

bool ImportTask::parseJson(const QString& path, QJsonObject& rootOut) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray data = f.readAll();
    f.close();
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return false;
    rootOut = doc.object();
    return true;
}

bool ImportTask::validateTasks(const QJsonObject& root, QJsonArray& tasksOut) {
    if (!root.contains("tasks") || !root.value("tasks").isArray()) return false;
    tasksOut = root.value("tasks").toArray();
    for (const auto& jt : tasksOut) {
        if (!jt.isObject()) return false;
        QJsonObject obj = jt.toObject();
        if (!obj.contains("id") || !obj.value("id").isString()) return false;
        if (!obj.contains("exec_command")) return false;
    }
    return true;
}

QVariantMap ImportTask::jsonObjectToStringMap(const QJsonObject& obj) const {
    QVariantMap m;
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        m[it.key()] = it.value().toString();
    }
    return m;
}

bool ImportTask::resolveConflicts(const QJsonArray& tasks, const QHash<QString, RectItem*>& existing, QHash<QString, RectItem*>& idToNode) {
    // 先将现有节点填入映射
    for (auto it = existing.begin(); it != existing.end(); ++it) {
        idToNode[it.key()] = it.value();
    }

    for (const auto& jt : tasks) {
        const QJsonObject obj = jt.toObject();
        const QString id = obj.value("id").toString();
        if (id.isEmpty()) return false;
        if (existing.contains(id)) {
            auto ret = QMessageBox::question(parentWidget_, tr("ID 冲突"),
                                             tr("节点 ID \"%1\" 已存在，是否复用现有节点？选择“否”将取消导入。").arg(id),
                                             QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (ret == QMessageBox::Yes) {
                idToNode[id] = existing.value(id);
                continue;
            } else {
                return false;
            }
        }
    }
    return true;
}

NodeType ImportTask::typeFromExec(const QString& execType) const {
    const QString t = execType.trimmed();
    if (t.compare("Shell", Qt::CaseInsensitive) == 0) return NodeType::Shell;
    if (t.compare("HttpCall", Qt::CaseInsensitive) == 0) return NodeType::Http;
    if (t.compare("Remote", Qt::CaseInsensitive) == 0) return NodeType::Remote;
    if (t.compare("Local", Qt::CaseInsensitive) == 0) return NodeType::Local;
    if (t.compare("Dag", Qt::CaseInsensitive) == 0) return NodeType::Dag;
    if (t.compare("Template", Qt::CaseInsensitive) == 0) return NodeType::Template;
    return NodeType::Local;
}
