// remote_rect_item.cpp
#include "remote_rect_item.h"
#include <QVariantMap>

namespace {
int gRemoteIdCounter = 1;
}

RemoteRectItem::RemoteRectItem(const QRectF& rect, QGraphicsItem* parent)
    : ContainerRectItem(rect, parent)
{
    setProp("id", QString("R_%1").arg(gRemoteIdCounter++));
    setProp("name", "Remote");
    setProp("exec_type", "Remote");
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

QString RemoteRectItem::typeLabel() const {
    return "Remote"; 
}
QColor  RemoteRectItem::headerColor() const { return QColor(120, 60, 170); }
QString RemoteRectItem::summaryText() const {
    return "";
}
