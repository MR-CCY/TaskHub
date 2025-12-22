#include "console_dock.h"

#include <QDateTime>
#include <QTabBar>
#include <QTabWidget>
#include <QTextEdit>

namespace {
void appendWithTs(QTextEdit* edit, bool allow, const QString& prefix, const QString& msg) {
    if (!allow || !edit) return;
    const QString ts = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    edit->append(QString("[%1] %2 %3").arg(ts, prefix, msg));
}
} // namespace

ConsoleDock::ConsoleDock(QWidget *parent)
    : QDockWidget(parent)
{
    tabs_ = new QTabWidget(this);
    consoleEdit_ = new QTextEdit(this);
    taskLogEdit_ = new QTextEdit(this);
    eventEdit_ = new QTextEdit(this);

    consoleEdit_->setReadOnly(true);
    taskLogEdit_->setReadOnly(true);
    eventEdit_->setReadOnly(true);
    
    // titleBarWidget()->setVisible(false);
    tabs_->addTab(consoleEdit_, tr("Console"));
    tabs_->addTab(taskLogEdit_, tr("Task Logs"));
    tabs_->addTab(eventEdit_, tr("WS Events"));
    // tabs_->setDocumentMode(true);
    // if (auto* bar = tabs_->tabBar()) {
    //     bar->setExpanding(true);
    // }

    setWidget(tabs_);
    setTitleBarWidget(new QWidget(this)); // hide dock title bar
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
    consoleEdit_->clear();
    taskLogEdit_->clear();
    eventEdit_->clear();
}

void ConsoleDock::appendLine(const QString &prefix, const QString &msg)
{
    appendWithTs(consoleEdit_, paused(), prefix, msg);
}

void ConsoleDock::appendInfo(const QString& msg) {
    appendLine("[I]", msg);
}

void ConsoleDock::appendError(const QString& msg) {
    appendLine("[E]", msg);
}

void ConsoleDock::appendTaskLog(const QString& msg) {
    appendWithTs(taskLogEdit_, paused(), "[Task]", msg);
}

void ConsoleDock::appendEvent(const QString& msg) {
    appendWithTs(eventEdit_, paused(), "[WS]", msg);
}
