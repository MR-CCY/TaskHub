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

    consoleEdit_->setReadOnly(true);
    taskLogEdit_->setReadOnly(true);
    eventEdit_->setReadOnly(true);
    
    // titleBarWidget()->setVisible(false);
    tabs_->addTab(consoleEdit_, tr("Console"));
    tabs_->addTab(taskLogEdit_, tr("Task Logs"));
    tabs_->addTab(eventEdit_, tr("WS Events"));
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
    consoleEdit_->clear();
    taskLogEdit_->clear();
    eventEdit_->clear();
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

QTextEdit* ConsoleDock::currentEdit() const {
    const int idx = tabs_ ? tabs_->currentIndex() : 0;
    switch (idx) {
    case 0: return consoleEdit_;
    case 1: return taskLogEdit_;
    case 2: return eventEdit_;
    default: return consoleEdit_;
    }
}

void ConsoleDock::filterCurrent(const QString& text) {
    QTextEdit* edit = currentEdit();
    if (!edit) return;
    QString source;
    switch (tabs_->currentIndex()) {
    case 0: source = consoleCache_; break;
    case 1: source = taskLogCache_; break;
    case 2: source = eventCache_; break;
    default: source = consoleCache_; break;
    }
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
    switch (tabs_->currentIndex()) {
    case 0: consoleCache_.clear(); consoleEdit_->clear(); break;
    case 1: taskLogCache_.clear(); taskLogEdit_->clear(); break;
    case 2: eventCache_.clear(); eventEdit_->clear(); break;
    default: break;
    }
}
