#pragma once
#include <QWidget>
#include <QVariant>

class QLabel;
class QLineEdit;

class NodeInspectorDagWidget : public QWidget {
public:
    explicit NodeInspectorDagWidget(QWidget* parent = nullptr);

    void setValues(const QVariantMap& exec);
    QString failPolicyValue() const;
    int maxParallelValue() const;
    void setReadOnlyMode(bool ro);

private:
    QLabel* dagJsonLabel_ = nullptr;
    QLineEdit* dagJsonEdit_ = nullptr;
    QLabel* failPolicyLabel_ = nullptr;
    class QComboBox* failPolicyCombo_ = nullptr;
    QLabel* maxParallelLabel_ = nullptr;
    class QSpinBox* maxParallelSpin_ = nullptr;
};
