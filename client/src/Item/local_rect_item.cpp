// local_rect_item.cpp
#include "local_rect_item.h"
#include <QVariantMap>

namespace {
int gLocalIdCounter = 1;
}

LocalRectItem::LocalRectItem(const QRectF& rect, QGraphicsItem* parent)
    : RectItem(rect, parent)
{
    QVariantMap execParams;
    execParams["handler"] = QString("handler");
    execParams["args"] = QString(""); // 仅存字符串；真实参数序列化由服务端解析

    props_["id"] = QString("L_%1").arg(gLocalIdCounter++);
    props_["name"] = "Local";
    props_["exec_type"] = "Local";
    props_["exec_command"] = QString("");
    props_["exec_params"] = execParams;
    props_["timeout_ms"] = static_cast<qint64>(0);
    props_["retry_count"] = 0;
    props_["retry_delay_ms"] = static_cast<qint64>(1000);
    props_["retry_exp_backoff"] = true;
    props_["priority"] = 0;
    props_["queue"] = "default";
    props_["capture_output"] = true;
    props_["metadata"] = QVariantMap{};
}

QString LocalRectItem::typeLabel() const {
    return "fx";
  }
QColor  LocalRectItem::headerColor() const { return QColor(220, 120, 30); }
QString LocalRectItem::summaryText() const {
    const QVariantMap execParams = props_.value("exec_params").toMap();
    const QString handler = execParams.value("handler", "").toString();
    return QString("%1").arg(handler);
}
