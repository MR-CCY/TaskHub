#pragma once
#include <QWidget>
#include <QVariant>

class QLabel;
class QLineEdit;
class QComboBox;
class QSpinBox;
class QComboBox;
class QPushButton;

class NodeInspectorRemoteWidget : public QWidget {
public:
    explicit NodeInspectorRemoteWidget(QWidget* parent = nullptr);

    void setValues(const QVariantMap& props, const QVariantMap& exec);
    QString queueValue() const;
    QString failPolicyValue() const;
    int maxParallelValue() const;
    void setReadOnlyMode(bool ro);

private:
    QLabel* queueLabel_ = nullptr;
    QLabel* failPolicyLabel_ = nullptr;
    QLabel* maxParallelLabel_ = nullptr;

    QComboBox* queueCombo_ = nullptr;
    QPushButton* moreBtn_ = nullptr;
    QComboBox* failPolicyCombo_ = nullptr;
    QSpinBox* maxParallelSpin_ = nullptr;
};
