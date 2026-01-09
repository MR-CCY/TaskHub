#include "dag_serializer.h"

#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QVariant>
#include <QList>
#include <QHash>

#include "Item/rect_item.h"
#include "Item/line_item.h"
#include "view/canvasscene.h"

#include "Item/remote_rect_item.h"
#include "Item/dag_rect_item.h"

namespace {
QJsonObject variantMapToJsonObject(const QVariantMap& m) {
    QJsonObject obj;
    for (auto it = m.constBegin(); it != m.constEnd(); ++it) {
        if (it.value().canConvert<QJsonObject>()) {
            obj[it.key()] = it.value().toJsonObject();
        } else if (it.value().canConvert<QJsonArray>()) {
            obj[it.key()] = it.value().toJsonArray();
        } else {
            // Use QJsonValue::fromVariant to preserve types (int, bool, etc)
            obj[it.key()] = QJsonValue::fromVariant(it.value());
        }
    }
    return obj;
}

QJsonObject serializeLevel(const QList<RectItem*>& nodes, const QJsonObject& config) {
    if (nodes.isEmpty()) return QJsonObject();

    QHash<RectItem*, QString> idMap;
    int autoId = 1;
    for (auto* n : nodes) {
        QString id = n->prop("id").toString();
        if (id.isEmpty()) {
            id = QString("node_%1").arg(autoId++);
        }
        idMap[n] = id;
    }

    QJsonArray tasks;
    for (auto* n : nodes) {
        QVariantMap props = n->properties();
        QJsonObject t;
        static const char* keys[] = {
            "id", "name", "exec_type", "exec_command", "exec_params",
            "timeout_ms", "retry_count", "retry_delay_ms", "retry_exp_backoff",
            "priority", "queue", "capture_output", "metadata"
        };
        for (auto key : keys) {
            QString skey = QString::fromUtf8(key);
            if (!props.contains(skey)) {
                if (skey == "exec_command") t[skey] = "";
                continue;
            }
            QVariant v = props.value(skey);
            if (skey == "exec_params") {
                QVariantMap execMap = v.toMap();
                
                // 处理容器节点的递归序列化
                bool isRemote = dynamic_cast<RemoteRectItem*>(n) != nullptr;
                bool isDagNode = dynamic_cast<DagRectItem*>(n) != nullptr;
                
                if (isRemote || isDagNode) {
                    QList<RectItem*> children;
                    for (auto* kid : n->childItems()) {
                        if (auto* r = dynamic_cast<RectItem*>(kid)) {
                            children.append(r);
                        }
                    }
                    if (!children.isEmpty()) {
                        QJsonObject subConfig;
                        subConfig["fail_policy"] = execMap.value("config.fail_policy", "SkipDownstream").toString();
                        subConfig["max_parallel"] = execMap.value("config.max_parallel", 4).toInt();
                        subConfig["name"] = n->prop("name").toString();
                        
                        QJsonObject subDag = serializeLevel(children, subConfig);
                        if (!subDag.isEmpty()) {
                            execMap["config"] = subDag.value("config").toObject();
                            execMap["tasks"] = subDag.value("tasks").toArray();
                            
                            // Remove flat redundant keys
                            execMap.remove("config.fail_policy");
                            execMap.remove("config.max_parallel");
                            execMap.remove("remote.fail_policy");
                            execMap.remove("remote.max_parallel");
                            execMap.remove("dag_json"); // Cleanup legacy field
                        }
                    }
                }
                
                t[skey] = variantMapToJsonObject(execMap);
            } else if (skey == "metadata") {
                // t[skey] = variantMapToStringObject(v.toMap());
            } else if (skey == "timeout_ms" || skey == "retry_delay_ms") {
                t[skey] = static_cast<qint64>(v.toLongLong());
            } else if (skey == "retry_count" || skey == "priority") {
                t[skey] = v.toInt();
            } else if (skey == "retry_exp_backoff" || skey == "capture_output") {
                t[skey] = v.toBool();
            } else {
                t[skey] = v.toString();
            }
        }

        QJsonArray deps;
        for (auto* line : n->lines()) {
            if (line->endItem() == n) {
                RectItem* src = line->startItem();
                if (src && idMap.contains(src)) {
                    deps.append(idMap[src]);
                }
            }
        }
        t["deps"] = deps;
        t["id"] = idMap.value(n);
        tasks.append(t);
    }

    QJsonObject root;
    root["config"] = config;
    root["tasks"] = tasks;
    return root;
}
}

QJsonObject buildDagJson(CanvasScene* scene, const QString& failPolicy, int maxParallel, const QString& dagName)
{
    if (!scene) return QJsonObject();
    
    // 只获取顶层节点
    QList<RectItem*> topNodes;
    for (auto* gitem : scene->items()) {
        if (auto* r = dynamic_cast<RectItem*>(gitem)) {
            // 如果父节点不是 RectItem，则视为顶层节点
            if (!dynamic_cast<RectItem*>(r->parentItem())) {
                topNodes.append(r);
            }
        }
    }
    
    QJsonObject config;
    config["fail_policy"] = failPolicy;
    config["max_parallel"] = maxParallel;
    if (!dagName.isEmpty()) config["name"] = dagName;

    return serializeLevel(topNodes, config);
}
