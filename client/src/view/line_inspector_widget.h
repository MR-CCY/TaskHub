#pragma once
#include <QWidget>

class QLabel;
class QPushButton;

// Line-level inspector form.
class LineInspectorWidget : public QWidget {
    Q_OBJECT
public:
    explicit LineInspectorWidget(QWidget* parent = nullptr);

    void setEndpoints(const QString& startName, const QString& endName);

signals:
    void chooseStartRequested();
    void chooseEndRequested();
    void saveRequested();

private:
    void buildUi();

private:
    QLabel* startLabel_ = nullptr;
    QLabel* endLabel_ = nullptr;
    QPushButton* chooseStartBtn_ = nullptr;
    QPushButton* chooseEndBtn_ = nullptr;
    QPushButton* saveBtn_ = nullptr;
};
