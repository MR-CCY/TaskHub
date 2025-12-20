#include "console_dock.h"
#include <QTextEdit>
#include <QDateTime>



ConsoleDock::ConsoleDock(QWidget *parent)
    : QDockWidget(tr("Console"), parent)
{
    edit_ = new QTextEdit(this);
    edit_->setReadOnly(true);
    setMaximumHeight(100);
    setWidget(edit_);
}
bool ConsoleDock::paused() const
{
    return paused_;
}

void ConsoleDock::setPaused(bool on)
{
    paused_ = on;
}
void ConsoleDock::clear()
{
    edit_->clear();
}
void ConsoleDock::appendLine(const QString &prefix, const QString &msg)
{
    if(!paused()) return;
    const QString ts = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    edit_->append(QString("[%1] %2 %3").arg(ts, prefix, msg));
}

void ConsoleDock::appendInfo(const QString& msg) {
    appendLine("[I]", msg);
}

void ConsoleDock::appendError(const QString& msg) {
    appendLine("[E]", msg);
}