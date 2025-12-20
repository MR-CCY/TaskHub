#pragma once
#include <QDockWidget>

class QTextEdit;

class ConsoleDock : public QDockWidget {
    Q_OBJECT
public:
   
    explicit ConsoleDock(QWidget* parent = nullptr);
    bool paused() const;
    void setPaused(bool on);
    void clear();
    void appendInfo(const QString& msg);
    void appendError(const QString& msg);

private:
    void appendLine(const QString& prefix, const QString& msg);

private:
    QTextEdit* edit_ = nullptr;
    bool paused_ = true;
};