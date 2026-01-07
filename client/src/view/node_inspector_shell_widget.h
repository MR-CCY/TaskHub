#pragma once
#include <QWidget>
#include <QVariant>

class QLabel;
class QLineEdit;

class NodeInspectorShellWidget : public QWidget {
public:
    explicit NodeInspectorShellWidget(QWidget* parent = nullptr);

    void setValues(const QVariantMap& exec);
    QString cwdValue() const;
    QString shellValue() const;
    void setReadOnlyMode(bool ro);

private:
    QLabel* cwdLabel_ = nullptr;
    QLabel* shellLabel_ = nullptr;
    QLineEdit* cwdEdit_ = nullptr;
    QLineEdit* shellEdit_ = nullptr;
};
