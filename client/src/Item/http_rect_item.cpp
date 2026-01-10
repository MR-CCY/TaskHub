// http_rect_item.cpp
#include "http_rect_item.h"
#include <QVariantMap>

namespace {
int gHttpIdCounter = 1;
}

HttpRectItem::HttpRectItem(const QRectF& rect, QGraphicsItem* parent)
    : RectItem(rect, parent)
{
    setProp("id", QString("H_%1").arg(gHttpIdCounter++));
    setProp("name", "HTTP");
    setProp("exec_type", "HttpCall");
    setProp("exec_command", "");
    setProp("timeout_ms", static_cast<qint64>(0));
    setProp("retry_count", 0);
    setProp("retry_delay_ms", static_cast<qint64>(1000));
    setProp("retry_exp_backoff", true);
    setProp("priority", 0);
    setProp("queue", "default");
    setProp("capture_output", true);
    setProp("metadata", QVariantMap{});
    setProp("description", "");

    setProp("exec_params.method", QString("GET"));
    setProp("exec_params.url", QString(""));
    setProp("exec_params.body", QString(""));
    setProp("exec_params.header.Content-Type", QString("application/json"));
    setProp("exec_params.follow_redirects", QString("true"));

    
}

QString HttpRectItem::typeLabel() const { 
    return "HTTP"; 
}
QColor  HttpRectItem::headerColor() const { return QColor(30, 140, 70); }
QString HttpRectItem::summaryText() const {
    const QString method = prop("exec_params.method").toString();
    const QString url    = prop("exec_command").toString();
    if (method.isEmpty()) return url;
    return QString("%1 %2").arg(method, url);
}
