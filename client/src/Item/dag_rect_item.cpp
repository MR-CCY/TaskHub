// dag_rect_item.cpp
#include "dag_rect_item.h"

#include <QVariantMap>

namespace {
int gDagIdCounter = 1;
}

DagRectItem::DagRectItem(const QRectF& rect, QGraphicsItem* parent)
    : ContainerRectItem(rect, parent)
{
    setProp("id", QString("DAG_%1").arg(gDagIdCounter++));
    setProp("name", "DAG");
    setProp("exec_type", "Dag");
    setProp("exec_command", QString(""));
    setProp("exec_params", QVariantMap{});
    setProp("timeout_ms", static_cast<qint64>(0));
    setProp("retry_count", 0);
    setProp("retry_delay_ms", static_cast<qint64>(1000));
    setProp("retry_exp_backoff", true);
    setProp("priority", 0);
    setProp("queue", "default");
    setProp("capture_output", true);
    setProp("metadata", QVariantMap{});
    setProp("exec_params.config.fail_policy", "SkipDownstream");
    setProp("exec_params.config.max_parallel", 4);
}

QString DagRectItem::typeLabel() const { return "DAG"; }
QColor  DagRectItem::headerColor() const { return QColor(50, 110, 190); }
QString DagRectItem::summaryText() const { return ""; }
