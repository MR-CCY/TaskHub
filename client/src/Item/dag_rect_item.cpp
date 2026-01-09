// dag_rect_item.cpp
#include "dag_rect_item.h"

#include <QVariantMap>

namespace {
int gDagIdCounter = 1;
}

DagRectItem::DagRectItem(const QRectF& rect, QGraphicsItem* parent)
    : ContainerRectItem(rect, parent)
{
    props_["id"] = QString("DAG_%1").arg(gDagIdCounter++); 
    props_["name"] = "DAG";
    props_["exec_type"] = "Dag";
    props_["exec_command"] = QString("");
    props_["exec_params"] = QVariantMap{};
    props_["timeout_ms"] = static_cast<qint64>(0);
    props_["retry_count"] = 0;
    props_["retry_delay_ms"] = static_cast<qint64>(1000);
    props_["retry_exp_backoff"] = true;
    props_["priority"] = 0;
    props_["queue"] = "default";
    props_["capture_output"] = true;
    props_["metadata"] = QVariantMap{};
    setPropByKeyPath("exec_params.config.fail_policy", "SkipDownstream");
    setPropByKeyPath("exec_params.config.max_parallel", 4);
}

QString DagRectItem::typeLabel() const { return "DAG"; }
QColor  DagRectItem::headerColor() const { return QColor(50, 110, 190); }
QString DagRectItem::summaryText() const { return ""; }
