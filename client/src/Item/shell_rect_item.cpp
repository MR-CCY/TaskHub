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

    props_["id"] = QString("S_%1").arg(gShellIdCounter++);
    props_["name"] = "Shell";
    props_["exec_type"] = "Shell";
    props_["exec_command"] = "";
    props_["timeout_ms"] = static_cast<qint64>(0);
    props_["retry_count"] = 0;
    props_["retry_delay_ms"] = static_cast<qint64>(1000);
    props_["retry_exp_backoff"] = true;
    props_["priority"] = 0;
    props_["queue"] = "default";
    props_["capture_output"] = true;
    props_["metadata"] = QVariantMap{};
    props_["description"] = "";

    // 使用 KeyPath 初始化嵌套参数
    setPropByKeyPath("exec_params.cmd", "echo hello");
    setPropByKeyPath("exec_params.cwd", "");
    setPropByKeyPath("exec_params.shell", "/bin/bash");
    setPropByKeyPath("exec_params.env", "");
}

QString ShellRectItem::typeLabel() const { 
    return ">_"; 
}
QColor  ShellRectItem::headerColor() const { return QColor(20, 70, 160); }
QString ShellRectItem::summaryText() const {
    const QString cmd = propByKeyPath("exec_params.cmd").toString();
    return QString("cmd: %1").arg(ellipsizeLine(cmd, 60));
}
