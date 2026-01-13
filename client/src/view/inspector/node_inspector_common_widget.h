#pragma once
#include <QVariant>
#include <QWidget>

class QComboBox;
class QCheckBox;
class QLabel;
class QLineEdit;

class NodeInspectorCommonWidget : public QWidget {
public:
    explicit NodeInspectorCommonWidget(QWidget* parent = nullptr);

    void setValues(const QVariantMap& props);

    QString nameValue() const;
    // QString commandValue() const;
    qint64 timeoutMsValue() const;
    int retryCountValue() const;
    int priorityValue() const;
    bool captureOutputValue() const;
    QString descriptionValue() const;    
    void setReadOnlyMode(bool ro);

private:
    QLabel* nameLabel_ = nullptr;
    QLabel* cmdLabel_ = nullptr;
    QLabel* timeoutLabel_ = nullptr;
    QLabel* retryLabel_ = nullptr;
    QLabel* priorityLabel_ = nullptr;
    QLabel* captureLabel_ = nullptr;
    QLabel* descriptionLabel_ = nullptr;

    QLineEdit* nameEdit_ = nullptr;
    QLineEdit* cmdEdit_ = nullptr;
    QLineEdit* timeoutEdit_ = nullptr;
    QLineEdit* retryEdit_ = nullptr;
    QComboBox* priorityCombo_ = nullptr;
    QCheckBox* captureBox_ = nullptr;
    QLineEdit* descriptionEdit_ = nullptr;
};
