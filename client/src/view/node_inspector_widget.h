#pragma once
#include <QWidget>
#include <QVariant>

class QLineEdit;
class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;

// Node-level inspector form.
class NodeInspectorWidget : public QWidget {
    Q_OBJECT
public:
    explicit NodeInspectorWidget(QWidget* parent = nullptr);

    void setValues(const QVariantMap& props, const QVariantMap& exec);

    QString nameValue() const;
    QString commandValue() const;
    qint64 timeoutMsValue() const;
    int retryCountValue() const;
    QString queueValue() const;
    bool captureOutputValue() const;
    QString httpMethodValue() const;
    QString httpBodyValue() const;
    QString shellCwdValue() const;
    QString shellShellValue() const;
    QString innerExecTypeValue() const;
    QString innerExecCommandValue() const;
    void setRuntimeValues(const QJsonObject& obj);
    void setReadOnlyMode(bool ro);

signals:
    void saveRequested();

private:
    void buildUi();
    void updateLabels(const QString& execType);
    void setFieldVisible(QLabel* label, QWidget* editor, bool visible);

private:
    QLabel* nameLabel_ = nullptr;
    QLabel* cmdLabel_ = nullptr;
    QLabel* timeoutLabel_ = nullptr;
    QLabel* retryLabel_ = nullptr;
    QLabel* queueLabel_ = nullptr;
    QLabel* captureLabel_ = nullptr;
    QLabel* httpMethodLabel_ = nullptr;
    QLabel* httpBodyLabel_ = nullptr;
    QLabel* shellCwdLabel_ = nullptr;
    QLabel* shellShellLabel_ = nullptr;
    QLabel* innerTypeLabel_ = nullptr;
    QLabel* innerCmdLabel_ = nullptr;

    QLineEdit* nameEdit_ = nullptr;
    QLineEdit* cmdEdit_ = nullptr;
    QLineEdit* timeoutEdit_ = nullptr;
    QLineEdit* retryEdit_ = nullptr;
    QLineEdit* queueEdit_ = nullptr;
    QCheckBox* captureBox_ = nullptr;
    QComboBox* httpMethodCombo_ = nullptr;
    QLineEdit* httpBodyEdit_ = nullptr;
    QLineEdit* shellCwdEdit_ = nullptr;
    QLineEdit* shellShellEdit_ = nullptr;
    QLineEdit* innerTypeEdit_ = nullptr;
    QLineEdit* innerCmdEdit_ = nullptr;
    QPushButton* saveBtn_ = nullptr;

    // runtime display
    QLabel* runtimeStatusLabel_ = nullptr;
    QLabel* runtimeDurationLabel_ = nullptr;
    QLabel* runtimeExitLabel_ = nullptr;
    QLabel* runtimeAttemptLabel_ = nullptr;
    QLabel* runtimeWorkerLabel_ = nullptr;
    QLabel* runtimeMessageLabel_ = nullptr;
    QLabel* runtimeStdoutLabel_ = nullptr;
    QLabel* runtimeStderrLabel_ = nullptr;

    bool readOnly_ = false;
};
