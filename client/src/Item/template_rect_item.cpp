// template_rect_item.cpp
#include "template_rect_item.h"

#include <QVariantMap>

namespace {
int gTemplateIdCounter = 1;
}

TemplateRectItem::TemplateRectItem(const QRectF& rect, QGraphicsItem* parent)
    : ContainerRectItem(rect, parent)
{
    props_["id"] = QString("TPL_%1").arg(gTemplateIdCounter++);
    props_["name"] = "Template";
    props_["exec_type"] = "Template";
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
}

QString TemplateRectItem::typeLabel() const { return "TPL"; }
QColor  TemplateRectItem::headerColor() const { return QColor(160, 90, 40); }
QString TemplateRectItem::summaryText() const { return props_.value("name", "").toString(); }
