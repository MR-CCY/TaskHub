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

namespace {
QJsonObject variantMapToStringObject(const QVariantMap& m) {
    QJsonObject obj;
    for (auto it = m.constBegin(); it != m.constEnd(); ++it) {
        obj[it.key()] = it.value().toString();
    }
    return obj;
}
}

QJsonObject buildDagJson(CanvasScene* scene, const QString& failPolicy, int maxParallel, const QString& dagName)
{
    if (!scene) return QJsonObject();
    QList<RectItem*> nodes;
    for (auto* gitem : scene->items()) {
        if (auto* r = dynamic_cast<RectItem*>(gitem)) {
            nodes.append(r);
        }
    }
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
            if (!props.contains(key)) continue;
            QVariant v = props.value(key);
            QString skey = QString::fromUtf8(key);
            if (skey == "exec_params") {
                t[skey] = variantMapToStringObject(v.toMap());
            } else if (skey == "metadata") {
                t[skey] = variantMapToStringObject(v.toMap());
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
    QJsonObject config;
    config["fail_policy"] = failPolicy;
    config["max_parallel"] = maxParallel;
    if (!dagName.isEmpty()) config["name"] = dagName;
    root["config"] = config;
    root["tasks"] = tasks;
    return root;
}
