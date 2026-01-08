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

    undo_->beginMacro("Import DAG");

    loadLevel(tasks, nullptr, idToNode);

    undo_->endMacro();
    if (auto* mgr = manager()) {
        auto* layoutTask = new LayoutTask(scene_, mgr);
        mgr->push(layoutTask);
        layoutTask->execute();
    }
    removeSelf();
}

void ImportTask::loadLevel(const QJsonArray& tasks, QGraphicsItem* parentNode, const QHash<QString, RectItem*>& preCreated) {
    QHash<QString, RectItem*> levelNodes = preCreated;

    // 1. 创建本层所有节点（如果还不存在的话）
    for (const auto& jt : tasks) {
        const QJsonObject obj = jt.toObject();
        const QString id = obj.value("id").toString();
        
        if (levelNodes.contains(id)) {
            // 已有节点可能来自冲突处理
            continue;
        }

        const QString execType = obj.value("exec_type").toString();
        NodeType nt = typeFromExec(execType);
        RectItem* node = NodeItemFactory::createNode(nt, QRectF(0, 0, 140, 140));
        if (!node) continue;

        // props 填充
        QVariantMap cfg;
        static const char* keys[] = {
            "id", "name", "exec_type", "exec_command", "exec_params",
            "timeout_ms", "retry_count", "retry_delay_ms", "retry_exp_backoff",
            "priority", "queue", "capture_output", "metadata", "description"
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
        
        if (parentNode) {
            node->setParentItem(parentNode);
        }
        
        node->execCreateCmd(true);
        levelNodes[id] = node;
    }

    // 2. 解析递归容器内容
    for (const auto& jt : tasks) {
        const QJsonObject obj = jt.toObject();
        const QString id = obj.value("id").toString();
        RectItem* node = levelNodes.value(id);
        if (!node) continue;

        QJsonObject execParams = obj.value("exec_params").toObject();
        if (execParams.contains("dag_json")) {
            inflateDagJson(node, execParams.value("dag_json").toString());
        }
    }

    // 3. 创建本层连线
    for (const auto& jt : tasks) {
        const QJsonObject obj = jt.toObject();
        const QString id = obj.value("id").toString();
        auto* target = levelNodes.value(id, nullptr);
        if (!target) continue;
        const QJsonArray deps = obj.value("deps").toArray();
        for (const auto& depVal : deps) {
            const QString depId = depVal.toString();
            auto* src = levelNodes.value(depId, nullptr);
            if (!src || src == target) continue;
            
            QGraphicsItem* lineParent = parentNode; // 连线与节点同级
            if (!LineItemFactory::canConnect(src, target, lineParent)) {
                continue;
            }
            auto* line = LineItemFactory::createLine(src, target, lineParent);
            if (!line) continue;
            line->attachContext(scene_, nullptr, undo_);
            line->execCreateCmd(true);
        }
    }
}

bool ImportTask::inflateDagJson(RectItem* node, const QString& dagJson) {
    if (dagJson.isEmpty()) return false;
    
    QJsonDocument doc = QJsonDocument::fromJson(dagJson.toUtf8());
    if (!doc.isObject()) return false;
    
    QJsonObject root = doc.object();
    if (!root.contains("tasks") || !root.value("tasks").isArray()) return false;
    
    QJsonArray tasks = root.value("tasks").toArray();
    loadLevel(tasks, node);
    return true;
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
