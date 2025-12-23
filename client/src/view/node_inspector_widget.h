#pragma once
#include <QWidget>
#include <QVariant>

class QLineEdit;
class QCheckBox;
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

signals:
    void saveRequested();

private:
    void buildUi();

private:
    QLineEdit* nameEdit_ = nullptr;
    QLineEdit* cmdEdit_ = nullptr;
    QLineEdit* timeoutEdit_ = nullptr;
    QLineEdit* retryEdit_ = nullptr;
    QLineEdit* queueEdit_ = nullptr;
    QCheckBox* captureBox_ = nullptr;
    QLineEdit* httpMethodEdit_ = nullptr;
    QLineEdit* httpBodyEdit_ = nullptr;
    QLineEdit* shellCwdEdit_ = nullptr;
    QLineEdit* shellShellEdit_ = nullptr;
    QLineEdit* innerTypeEdit_ = nullptr;
    QLineEdit* innerCmdEdit_ = nullptr;
    QPushButton* saveBtn_ = nullptr;
};
