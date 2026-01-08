// http_rect_item.cpp
#include "http_rect_item.h"
#include <QVariantMap>

namespace {
int gHttpIdCounter = 1;
}

HttpRectItem::HttpRectItem(const QRectF& rect, QGraphicsItem* parent)
    : RectItem(rect, parent)
{
    props_["id"] = QString("H_%1").arg(gHttpIdCounter++);
    props_["name"] = "HTTP";
    props_["exec_type"] = "HttpCall";
    props_["timeout_ms"] = static_cast<qint64>(0);
    props_["retry_count"] = 0;
    props_["retry_delay_ms"] = static_cast<qint64>(1000);
    props_["retry_exp_backoff"] = true;
    props_["priority"] = 0;
    props_["queue"] = "default";
    props_["capture_output"] = true;
    props_["metadata"] = QVariantMap{};
    props_["description"] = "";

    setPropByKeyPath("exec_params.method", QString("GET"));
    setPropByKeyPath("exec_params.url", QString(""));
    setPropByKeyPath("exec_params.body", QString(""));
    setPropByKeyPath("exec_params.header.Content-Type", QString("application/json"));
    setPropByKeyPath("exec_params.follow_redirects", QString("true"));

    
}

QString HttpRectItem::typeLabel() const { 
    return "HTTP"; 
}
QColor  HttpRectItem::headerColor() const { return QColor(30, 140, 70); }
QString HttpRectItem::summaryText() const {
    const QVariantMap execParams = props_.value("exec_params").toMap();
    const QString method = execParams.value("method", "GET").toString();
    const QString url    = props_.value("exec_command", "").toString();
    return QString("%1 %2").arg(method, url);
}
