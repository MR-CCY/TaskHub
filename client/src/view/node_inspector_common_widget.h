#pragma once
#include <QVariant>
#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;

class NodeInspectorCommonWidget : public QWidget {
public:
    explicit NodeInspectorCommonWidget(QWidget* parent = nullptr);

    void setValues(const QVariantMap& props);
    void setCommandLabel(const QString& text);

    QString nameValue() const;
    QString commandValue() const;
    qint64 timeoutMsValue() const;
    int retryCountValue() const;
    QString queueValue() const;
    bool captureOutputValue() const;

    void setReadOnlyMode(bool ro);

private:
    QLabel* nameLabel_ = nullptr;
    QLabel* cmdLabel_ = nullptr;
    QLabel* timeoutLabel_ = nullptr;
    QLabel* retryLabel_ = nullptr;
    QLabel* queueLabel_ = nullptr;
    QLabel* captureLabel_ = nullptr;

    QLineEdit* nameEdit_ = nullptr;
    QLineEdit* cmdEdit_ = nullptr;
    QLineEdit* timeoutEdit_ = nullptr;
    QLineEdit* retryEdit_ = nullptr;
    QLineEdit* queueEdit_ = nullptr;
    QCheckBox* captureBox_ = nullptr;
};
