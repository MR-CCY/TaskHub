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

    setProp("id", QString("S_%1").arg(gShellIdCounter++));
    setProp("name", "Shell");
    setProp("exec_type", "Shell");
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

    // 使用 KeyPath 初始化嵌套参数
    setProp("exec_params.cmd", "echo hello");
    setProp("exec_params.cwd", "");
    setProp("exec_params.shell", "/bin/bash");
    setProp("exec_params.env", "");
}

QString ShellRectItem::typeLabel() const { 
    return ">_"; 
}
QColor  ShellRectItem::headerColor() const { return QColor(20, 70, 160); }
QString ShellRectItem::summaryText() const {
    const QString cmd = propByKeyPath("exec_params.cmd").toString();
    return QString("cmd: %1").arg(ellipsizeLine(cmd, 60));
}
