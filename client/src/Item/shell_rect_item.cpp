// shell_rect_item.cpp
#include "shell_rect_item.h"

#include <QVariantMap>

static QString ellipsizeLine(const QString& s, int maxChars) {
    if (s.size() <= maxChars) return s;
    return s.left(maxChars - 1) + "…";
}

namespace {
int gShellIdCounter = 1;
}

ShellRectItem::ShellRectItem(const QRectF& rect, QGraphicsItem* parent)
    : RectItem(rect, parent)
{
    QVariantMap execParams;
    execParams["cwd"] = QString("");
    execParams["shell"] = QString("/bin/bash");
    execParams["env.PATH"] = QString("");

    props_["id"] = QString("S_%1").arg(gShellIdCounter++);
    props_["name"] = "Shell";
    props_["exec_type"] = "Shell";
    props_["exec_command"] = "echo hello";
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

QString ShellRectItem::typeLabel() const { 
    return ">_"; 
}
QColor  ShellRectItem::headerColor() const { return QColor(20, 70, 160); }
QString ShellRectItem::summaryText() const {
    const QString cmd = props_.value("exec_command").toString();
    return QString("cmd: %1").arg(ellipsizeLine(cmd, 60));
}
