#pragma once
#include <QWidget>
#include <QVariant>

class QLabel;
class QLineEdit;
class QComboBox;
class QPushButton;

class NodeInspectorRemoteWidget : public QWidget {
public:
    explicit NodeInspectorRemoteWidget(QWidget* parent = nullptr);

    void setValues(const QVariantMap& props, const QVariantMap& exec);
    QString innerTypeValue() const;
    QString innerCmdValue() const;
    QString queueValue() const;
    void setReadOnlyMode(bool ro);

private:
    QLabel* innerTypeLabel_ = nullptr;
    QLabel* innerCmdLabel_ = nullptr;
    QLabel* queueLabel_ = nullptr;

    QLineEdit* innerTypeEdit_ = nullptr;
    QLineEdit* innerCmdEdit_ = nullptr;
    QComboBox* queueCombo_ = nullptr;
    QPushButton* moreBtn_ = nullptr;
};
