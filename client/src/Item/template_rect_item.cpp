// template_rect_item.cpp
#include "template_rect_item.h"

#include <QVariantMap>

namespace {
int gTemplateIdCounter = 1;
}

TemplateRectItem::TemplateRectItem(const QRectF& rect, QGraphicsItem* parent)
    : ContainerRectItem(rect, parent)
{
    setProp("id", QString("TPL_%1").arg(gTemplateIdCounter++));
    setProp("name", "Template");
    setProp("exec_type", "Template");
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
}

QString TemplateRectItem::typeLabel() const { return "TPL"; }
QColor  TemplateRectItem::headerColor() const { return QColor(160, 90, 40); }
QString TemplateRectItem::summaryText() const { return prop("name").toString(); }
