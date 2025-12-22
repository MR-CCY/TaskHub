#pragma once
#include <QDockWidget>

class QTextEdit;
class QTabWidget;

class ConsoleDock : public QDockWidget {
    Q_OBJECT
public:
   
    explicit ConsoleDock(QWidget* parent = nullptr);
    bool paused() const;
    void setPaused(bool on);
    void clear();
    void appendInfo(const QString& msg);
    void appendError(const QString& msg);
    void appendTaskLog(const QString& msg);
    void appendEvent(const QString& msg);

private:
    void appendLine(const QString& prefix, const QString& msg);

private:
    QTabWidget* tabs_ = nullptr;
    QTextEdit* consoleEdit_ = nullptr;
    QTextEdit* taskLogEdit_ = nullptr;
    QTextEdit* eventEdit_ = nullptr;
    bool paused_ = true;
};
