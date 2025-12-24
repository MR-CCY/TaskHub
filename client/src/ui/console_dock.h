#pragma once
#include <QDockWidget>

class QTextEdit;
class QTabWidget;
class QLineEdit;
class QPushButton;
class QWidget;

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
    void filterCurrent(const QString& text);
    void clearCurrent();

private:
    QTextEdit* currentEdit() const;
    void appendLine(const QString& prefix, const QString& msg);

private:
    QWidget* container_ = nullptr;
    QLineEdit* searchEdit_ = nullptr;
    QPushButton* clearBtn_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    QTextEdit* consoleEdit_ = nullptr;
    QTextEdit* taskLogEdit_ = nullptr;
    QTextEdit* eventEdit_ = nullptr;
    QString consoleCache_;
    QString taskLogCache_;
    QString eventCache_;
    bool paused_ = true;
};
