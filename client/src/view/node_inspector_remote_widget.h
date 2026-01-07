#pragma once
#include <QWidget>
#include <QVariant>

class QLabel;
class QLineEdit;

class NodeInspectorRemoteWidget : public QWidget {
public:
    explicit NodeInspectorRemoteWidget(QWidget* parent = nullptr);

    void setValues(const QVariantMap& exec);
    QString innerTypeValue() const;
    QString innerCmdValue() const;
    void setReadOnlyMode(bool ro);

private:
    QLabel* innerTypeLabel_ = nullptr;
    QLabel* innerCmdLabel_ = nullptr;
    QLineEdit* innerTypeEdit_ = nullptr;
    QLineEdit* innerCmdEdit_ = nullptr;
};
