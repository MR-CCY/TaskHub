#include "console_dock.h"

#include <QDateTime>
#include <QTabBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>

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
    container_ = new QWidget(this);
    auto* vbox = new QVBoxLayout(container_);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->setSpacing(4);

    auto* topBar = new QHBoxLayout();
    topBar->setSpacing(6);
    searchEdit_ = new QLineEdit(container_);
    searchEdit_->setPlaceholderText(tr("搜索当前标签..."));
    clearBtn_ = new QPushButton(tr("清空"), container_);
    topBar->addWidget(searchEdit_, 1);
    topBar->addWidget(clearBtn_);
    vbox->addLayout(topBar);

    tabs_ = new QTabWidget(container_);
    consoleEdit_ = new QTextEdit(this);
    taskLogEdit_ = new QTextEdit(this);
    eventEdit_ = new QTextEdit(this);
    timelineEdit_ = new QTextEdit(this);

    consoleEdit_->setReadOnly(true);
    taskLogEdit_->setReadOnly(true);
    eventEdit_->setReadOnly(true);
    timelineEdit_->setReadOnly(true);
    
    // titleBarWidget()->setVisible(false);
    tabs_->addTab(consoleEdit_, tr("Console"));
    tabs_->addTab(taskLogEdit_, tr("Task Logs"));
    tabs_->addTab(eventEdit_, tr("WS Events"));
    tabs_->addTab(timelineEdit_, tr("Timeline"));
    tabs_->setDocumentMode(true);
    // if (auto* bar = tabs_->tabBar()) {
    //     bar->setExpanding(true);
    // }

    vbox->addWidget(tabs_);

    setWidget(container_);
    setTitleBarWidget(new QWidget(this)); // hide dock title bar

    connect(searchEdit_, &QLineEdit::textChanged, this, &ConsoleDock::filterCurrent);
    connect(clearBtn_, &QPushButton::clicked, this, &ConsoleDock::clearCurrent);
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
    consoleCache_.clear();
    taskLogCache_.clear();
    eventCache_.clear();
    timelineCache_.clear();
    consoleEdit_->clear();
    taskLogEdit_->clear();
    eventEdit_->clear();
    timelineEdit_->clear();
}

void ConsoleDock::appendLine(const QString &prefix, const QString &msg)
{
    const QString line = QString("[%1] %2 %3").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"), prefix, msg);
    consoleCache_.append(line + "\n");
    appendWithTs(consoleEdit_, paused(), prefix, msg);
}

void ConsoleDock::appendInfo(const QString& msg) {
    appendLine("[I]", msg);
}

void ConsoleDock::appendError(const QString& msg) {
    appendLine("[E]", msg);
}

void ConsoleDock::appendTaskLog(const QString& msg) {
    const QString line = QString("[%1] %2 %3").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"), "[Task]", msg);
    taskLogCache_.append(line + "\n");
    appendWithTs(taskLogEdit_, paused(), "[Task]", msg);
}

void ConsoleDock::appendEvent(const QString& msg) {
    const QString line = QString("[%1] %2 %3").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"), "[WS]", msg);
    eventCache_.append(line + "\n");
    appendWithTs(eventEdit_, paused(), "[WS]", msg);
}
void ConsoleDock::appendTimeline(const QString& msg) {
    const QString line = QString("[%1] %2 %3").arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"), "[Timeline]", msg);
    timelineCache_.append(line + "\n");
    appendWithTs(timelineEdit_, paused(), "[Timeline]", msg);
}

void ConsoleDock::hideConsoleTab() {
    if (!tabs_) return;
    tabs_->removeTab(0);
}

QTextEdit* ConsoleDock::currentEdit() const {
    return tabs_ ? qobject_cast<QTextEdit*>(tabs_->currentWidget()) : nullptr;
}

void ConsoleDock::filterCurrent(const QString& text) {
    QTextEdit* edit = currentEdit();
    if (!edit) return;
    QString source;
    if (edit == consoleEdit_) source = consoleCache_;
    else if (edit == taskLogEdit_) source = taskLogCache_;
    else if (edit == eventEdit_) source = eventCache_;
    else if (edit == timelineEdit_) source = timelineCache_;
    else return;
    if (text.isEmpty()) {
        edit->setPlainText(source);
        edit->moveCursor(QTextCursor::End);
        return;
    }
    QStringList lines = source.split('\n', Qt::SkipEmptyParts);
    QStringList filtered;
    for (const auto& line : lines) {
        if (line.contains(text, Qt::CaseInsensitive)) {
            filtered << line;
        }
    }
    edit->setPlainText(filtered.join("\n"));
    edit->moveCursor(QTextCursor::End);
}

void ConsoleDock::clearCurrent() {
    QTextEdit* edit = currentEdit();
    if (edit == consoleEdit_) { consoleCache_.clear(); consoleEdit_->clear(); }
    else if (edit == taskLogEdit_) { taskLogCache_.clear(); taskLogEdit_->clear(); }
    else if (edit == eventEdit_) { eventCache_.clear(); eventEdit_->clear(); }
    else if (edit == timelineEdit_) { timelineCache_.clear(); timelineEdit_->clear(); }
}
