#include "dag_loader.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "view/canvasscene.h"
#include "commands/undostack.h"
#include "Item/rect_item.h"
#include "Item/line_item.h"
#include "Item/line_item_factory.h"
#include "Item/node_item_factory.h"
#include "Item/node_type.h"

namespace {
    NodeType typeFromExec(const QString& execType) {
        const QString t = execType.trimmed();
        if (t.compare("Shell", Qt::CaseInsensitive) == 0) return NodeType::Shell;
        if (t.compare("HttpCall", Qt::CaseInsensitive) == 0) return NodeType::Http;
        if (t.compare("Remote", Qt::CaseInsensitive) == 0) return NodeType::Remote;
        if (t.compare("Local", Qt::CaseInsensitive) == 0) return NodeType::Local;
        if (t.compare("Dag", Qt::CaseInsensitive) == 0) return NodeType::Dag;
        if (t.compare("Template", Qt::CaseInsensitive) == 0) return NodeType::Template;
        // fallback
        const QString tl = t.toLower();
        if (tl == "http") return NodeType::Http;
        return NodeType::Local;
    }

    QVariantMap jsonObjectToStringMap(const QJsonObject& obj) {
        QVariantMap m;
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            m[it.key()] = it.value().toString();
        }
        return m;
    }
}

void DagLoader::loadLevel(const QJsonArray& tasks, CanvasScene* scene, UndoStack* undo, QGraphicsItem* parentNode,const QHash<QString, RectItem*>& preExisting,QHash<QString, RectItem*>& outCreatedNodes,const QString& pathPrefix)
{
    QHash<QString, RectItem*> levelNodes = preExisting;

    // 1. 创建本层所有节点（如果还不存在的话）
    for (const auto& jt : tasks) {
        const QJsonObject obj = jt.toObject();
        const QString id = obj.value("id").toString();
        // Construct full logical path for global lookup
        QString fullId = id;
        if (!pathPrefix.isEmpty()) {
            fullId = pathPrefix + "/" + id;
        }

        if (levelNodes.contains(id)) {
            // 已有节点可能来自冲突处理
            // 也要加入到 output，方便调用者知道哪些节点在其控制下
            outCreatedNodes[fullId] = levelNodes[id];
            continue;
        }

        const QString execType = obj.value("exec_type").toString("Local");
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
            // logic aligned with ImportTask and DagRunViewer
            if (sk == "exec_params" || sk == "metadata") {
                 cfg[sk] = obj.value(sk).toVariant(); // generic
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
       
        // Let's use simple QVariant conversion from QJsonValue.
        if (obj.contains("exec_params")) {
             cfg["exec_params"] = obj.value("exec_params").toVariant();
        }
        if (obj.contains("metadata")) {
             cfg["metadata"] = obj.value("metadata").toVariant();
        }

        node->setTaskConfig(cfg);
        node->attachContext(scene, nullptr, undo);
        
        if (parentNode) {
            node->setParentItem(parentNode);
        }
        
        node->execCreateCmd(true);
        levelNodes[id] = node;
        outCreatedNodes[fullId] = node;
    }

    // 2. 解析递归容器内容
    for (const auto& jt : tasks) {
        const QJsonObject obj = jt.toObject();
        const QString id = obj.value("id").toString();
        RectItem* node = levelNodes.value(id);
        if (!node) continue;

        QJsonObject execParams = obj.value("exec_params").toObject();
        QString currentFullId = id;
        if (!pathPrefix.isEmpty()) {
            currentFullId = pathPrefix + "/" + id;
        }

        if (execParams.contains("tasks") && execParams.value("tasks").isArray()) {
            // New structured format
            inflateDagObject(node, execParams, scene, undo, outCreatedNodes, currentFullId);
        } else if (execParams.contains("dag_json")) {
            // Legacy dag_json string
            inflateDagJson(node, execParams.value("dag_json").toString(), scene, undo, outCreatedNodes, currentFullId);
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
            line->attachContext(scene, nullptr, undo);
            line->execCreateCmd(true);
        }
    }
}

bool DagLoader::inflateDagJson(RectItem* node, const QString& dagJson, CanvasScene* scene, UndoStack* undo, QHash<QString, RectItem*>& outCreatedNodes, const QString& pathPrefix) {
    if (dagJson.isEmpty()) return false;
    
    QJsonDocument doc = QJsonDocument::fromJson(dagJson.toUtf8());
    if (!doc.isObject()) return false;
    
    return inflateDagObject(node, doc.object(), scene, undo, outCreatedNodes, pathPrefix);
}

bool DagLoader::inflateDagObject(RectItem* node, const QJsonObject& dagObj, CanvasScene* scene, UndoStack* undo, QHash<QString, RectItem*>& outCreatedNodes, const QString& pathPrefix) {
    if (dagObj.isEmpty()) return false;
    if (!dagObj.contains("tasks") || !dagObj.value("tasks").isArray()) return false;
    
    QJsonArray tasks = dagObj.value("tasks").toArray();
    
    // Recursive loading
    QHash<QString, RectItem*> dummyPreExisting;
    loadLevel(tasks, scene, undo, node, dummyPreExisting, outCreatedNodes, pathPrefix);
    return true;
}
