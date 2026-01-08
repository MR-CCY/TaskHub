// remote_rect_item.cpp
#include "remote_rect_item.h"
#include <QVariantMap>

namespace {
int gRemoteIdCounter = 1;
}

RemoteRectItem::RemoteRectItem(const QRectF& rect, QGraphicsItem* parent)
    : ContainerRectItem(rect, parent)
{
    props_["id"] = QString("R_%1").arg(gRemoteIdCounter++);
    props_["name"] = "Remote";
    props_["exec_type"] = "Remote";
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
    setPropByKeyPath("exec_params.remote.fail_policy", "SkipDownstream");
    setPropByKeyPath("exec_params.remote.max_parallel", 4);
}

QString RemoteRectItem::typeLabel() const {
    return "Remote"; 
}
QColor  RemoteRectItem::headerColor() const { return QColor(120, 60, 170); }
QString RemoteRectItem::summaryText() const {
    return "";
}
