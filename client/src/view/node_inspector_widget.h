#pragma once
#include <QVariant>
#include <QWidget>

class QJsonObject;
class QStackedWidget;
class NodeInspectorCommonWidget;
class NodeInspectorHttpWidget;
class NodeInspectorLocalWidget;
class NodeInspectorShellWidget;
class NodeInspectorRemoteWidget;
class NodeInspectorDagWidget;
class NodeInspectorTemplateWidget;
class NodeInspectorActionWidget;
class NodeInspectorRuntimeWidget;

// Node-level inspector form.
class NodeInspectorWidget : public QWidget {
    Q_OBJECT
public:
    explicit NodeInspectorWidget(QWidget* parent = nullptr);

    void setValues(const QVariantMap& props, const QVariantMap& exec);

    QString nameValue() const;
    // QString commandValue() const;
    qint64 timeoutMsValue() const;
    int retryCountValue() const;
    int priorityValue() const;
    QString queueValue() const;
    bool captureOutputValue() const;
    QString httpMethodValue() const;
    QString httpUrlValue() const;
    QString httpHeadersValue() const;
    QString httpBodyValue() const;
    QString httpAuthUserValue() const;
    QString httpAuthPassValue() const;
    bool httpFollowRedirectsValue() const;
    QString localHandlerValue() const;
    QString descriptionValue() const;
    
    QString shellCwdValue() const;
    QString shellShellValue() const;
    QString shellENVValue() const;
    QString shellCmdValue() const;

    QString innerExecTypeValue() const;
    QString innerExecCommandValue() const;
    QString dagJsonValue() const;
    QString templateIdValue() const;
    QString templateParamsJsonValue() const;
    QString cronSpecValue() const;
    void setRuntimeValues(const QJsonObject& obj);
    void setReadOnlyMode(bool ro);

signals:
    void saveRequested();
    void cronCreateRequested(const QString& spec);

private:
    void buildUi();
    void updateExecSection(const QString& execType);

private:
    NodeInspectorCommonWidget* common_ = nullptr;
    QStackedWidget* execStack_ = nullptr;
    QWidget* emptyExec_ = nullptr;
    NodeInspectorLocalWidget* local_ = nullptr;
    NodeInspectorHttpWidget* http_ = nullptr;
    NodeInspectorShellWidget* shell_ = nullptr;
    NodeInspectorRemoteWidget* remote_ = nullptr;
    NodeInspectorDagWidget* dag_ = nullptr;
    NodeInspectorTemplateWidget* template_ = nullptr;
    NodeInspectorActionWidget* actions_ = nullptr;
    NodeInspectorRuntimeWidget* runtime_ = nullptr;

    bool readOnly_ = false;
};
