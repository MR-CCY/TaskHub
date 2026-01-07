#pragma once
#include <QWidget>

class QPushButton;
class QTimeEdit;

class NodeInspectorActionWidget : public QWidget {
public:
    explicit NodeInspectorActionWidget(QWidget* parent = nullptr);

    QPushButton* saveButton() const;
    QPushButton* cronButton() const;
    QString cronSpecValue() const;
    void setReadOnlyMode(bool ro);

private:
    QPushButton* saveBtn_ = nullptr;
    QTimeEdit* cronTimeEdit_ = nullptr;
    QPushButton* cronBtn_ = nullptr;
};
