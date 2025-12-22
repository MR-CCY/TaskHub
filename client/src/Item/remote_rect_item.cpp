// remote_rect_item.cpp
#include "remote_rect_item.h"
#include <QVariantMap>

namespace {
int gRemoteIdCounter = 1;
}

RemoteRectItem::RemoteRectItem(const QRectF& rect, QGraphicsItem* parent)
    : RectItem(rect, parent)
{
    QVariantMap execParams;
    execParams["inner.exec_type"] = QString("Shell");
    execParams["inner.exec_command"] = QString("echo remote");
    execParams["inner.exec_params.cwd"] = QString("");
    execParams["inner.exec_params.shell"] = QString("/bin/bash");

    props_["id"] = QString("R_%1").arg(gRemoteIdCounter++);
    props_["name"] = "Remote";
    props_["exec_type"] = "Remote";
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

QString RemoteRectItem::typeLabel() const {
    return "SSH"; 
}
QColor  RemoteRectItem::headerColor() const { return QColor(120, 60, 170); }
QString RemoteRectItem::summaryText() const {
    const QVariantMap execParams = props_.value("exec_params").toMap();
    const QString innerType = execParams.value("inner.exec_type", "").toString();
    const QString innerCmd  = execParams.value("inner.exec_command", "").toString();
    return QString("%1 %2").arg(innerType, innerCmd);
}
