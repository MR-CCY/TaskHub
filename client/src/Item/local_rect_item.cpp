// local_rect_item.cpp
#include "local_rect_item.h"
#include <QVariantMap>

namespace {
int gLocalIdCounter = 1;
}

LocalRectItem::LocalRectItem(const QRectF& rect, QGraphicsItem* parent)
    : RectItem(rect, parent)
{
    setProp("id", QString("L_%1").arg(gLocalIdCounter++));
    setProp("name", "Local");
    setProp("exec_type", "Local");
    setProp("exec_command", QString(""));
    setProp("exec_params.handler", QString("handler"));
    setProp("exec_params.args", QString(""));
    setProp("timeout_ms", static_cast<qint64>(0));
    setProp("retry_count", 0);
    setProp("retry_delay_ms", static_cast<qint64>(1000));
    setProp("retry_exp_backoff", true);
    setProp("priority", 0);
    setProp("queue", "default");
    setProp("capture_output", true);
    setProp("metadata", QVariantMap{});
}

QString LocalRectItem::typeLabel() const {
    return "fx";
  }
QColor  LocalRectItem::headerColor() const { return QColor(220, 120, 30); }
QString LocalRectItem::summaryText() const {
    return prop("exec_params.handler").toString();
}
