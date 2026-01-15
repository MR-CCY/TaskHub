#include "node_inspector_widget.h"

#include <QJsonObject>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "node_inspector_action_widget.h"
#include "node_inspector_common_widget.h"
#include "node_inspector_dag_widget.h"
#include "node_inspector_http_widget.h"
#include "node_inspector_local_widget.h"
#include "node_inspector_remote_widget.h"
#include "node_inspector_runtime_widget.h"
#include "node_inspector_shell_widget.h"
#include "node_inspector_template_widget.h"

namespace {

enum class ExecKind { Local, Remote, Script, Http, Shell, Dag, Template };

ExecKind toExecKind(const QString& s) {
    const QString t = s.trimmed().toLower();
    if (t == "httpcall" || t == "http_call" || t == "http") return ExecKind::Http;
    if (t == "shell") return ExecKind::Shell;
    if (t == "remote") return ExecKind::Remote;
    if (t == "dag") return ExecKind::Dag;
    if (t == "template") return ExecKind::Template;
    if (t == "script") return ExecKind::Script;
    return ExecKind::Local;
}

} // namespace

NodeInspectorWidget::NodeInspectorWidget(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void NodeInspectorWidget::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    common_ = new NodeInspectorCommonWidget(this);
    layout->addWidget(common_);

    execStack_ = new QStackedWidget(this);
    execStack_->setContentsMargins(0, 0, 0, 0);
    execStack_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    emptyExec_ = new QWidget(execStack_);
    local_ = new NodeInspectorLocalWidget(execStack_);
    http_ = new NodeInspectorHttpWidget(execStack_);
    shell_ = new NodeInspectorShellWidget(execStack_);
    remote_ = new NodeInspectorRemoteWidget(execStack_);
    dag_ = new NodeInspectorDagWidget(execStack_);
    template_ = new NodeInspectorTemplateWidget(execStack_);

    execStack_->addWidget(emptyExec_);
    execStack_->addWidget(local_);
    execStack_->addWidget(http_);
    execStack_->addWidget(shell_);
    execStack_->addWidget(remote_);
    execStack_->addWidget(dag_);
    execStack_->addWidget(template_);

    layout->addWidget(execStack_);

    actions_ = new NodeInspectorActionWidget(this);
    layout->addWidget(actions_);

    runtime_ = new NodeInspectorRuntimeWidget(this);
    layout->addWidget(runtime_);

    connect(actions_->saveButton(), &QPushButton::clicked, this, &NodeInspectorWidget::saveRequested);
    connect(actions_->cronButton(), &QPushButton::clicked, this, [this]() {
        emit cronCreateRequested(cronSpecValue());
    });

    updateExecSection(QString());
}

void NodeInspectorWidget::setValues(const QVariantMap& props, const QVariantMap& exec)
{
    if (common_) common_->setValues(props);
    if (local_) local_->setValues(props, exec);
    if (http_) http_->setValues(exec);
    if (shell_) shell_->setValues(exec);
    if (remote_) remote_->setValues(props, exec);
    if (dag_) dag_->setValues(exec);
    if (template_) template_->setValues(exec);
    updateExecSection(props.value("exec_type").toString());
}

void NodeInspectorWidget::setApiClient(ApiClient* api)
{
    api_ = api;
    if (template_) template_->setApiClient(api);
}

QString NodeInspectorWidget::nameValue() const {
    return common_ ? common_->nameValue() : QString();
}


qint64 NodeInspectorWidget::timeoutMsValue() const {
    return common_ ? common_->timeoutMsValue() : 0;
}

QString NodeInspectorWidget::descriptionValue() const {
    return common_ ? common_->descriptionValue() : QString();
}

int NodeInspectorWidget::retryCountValue() const {
    return common_ ? common_->retryCountValue() : 0;
}

int NodeInspectorWidget::priorityValue() const {
    return common_ ? common_->priorityValue() : 0;
}

QString NodeInspectorWidget::queueValue() const {
    if (remote_ && execStack_ && execStack_->currentWidget() == remote_) {
        return remote_->queueValue();
    }
    return QString(); // 只有 Remote 节点目前有 queue 选择
}

bool NodeInspectorWidget::captureOutputValue() const {
    return common_ ? common_->captureOutputValue() : false;
}

QString NodeInspectorWidget::httpMethodValue() const {
    return http_ ? http_->methodValue() : QString();
}

QString NodeInspectorWidget::httpUrlValue() const {
    return http_ ? http_->urlValue() : QString();
}

QString NodeInspectorWidget::httpHeadersValue() const {
    return http_ ? http_->headersValue() : QString();
}

QString NodeInspectorWidget::httpBodyValue() const {
    return http_ ? http_->bodyValue() : QString();
}

QString NodeInspectorWidget::httpAuthUserValue() const {
    return http_ ? http_->authUserValue() : QString();
}

QString NodeInspectorWidget::httpAuthPassValue() const {
    return http_ ? http_->authPassValue() : QString();
}

bool NodeInspectorWidget::httpFollowRedirectsValue() const {
    return http_ ? http_->followRedirectsValue() : true;
}

QString NodeInspectorWidget::localHandlerValue() const {
    return local_ ? local_->handlerValue() : QString();
}

QString NodeInspectorWidget::shellCwdValue() const {
    return shell_ ? shell_->cwdValue() : QString();
}

QString NodeInspectorWidget::shellShellValue() const {
    return shell_ ? shell_->shellValue() : QString();
}

QString NodeInspectorWidget::shellENVValue() const
{
    return shell_ ? shell_->envValue() : QString();
}

QString NodeInspectorWidget::shellCmdValue() const
{
    return shell_ ? shell_->cmdValue() : QString();
}

QString NodeInspectorWidget::dagFailPolicyValue() const {
    return dag_ ? dag_->failPolicyValue() : QString();
}

int NodeInspectorWidget::dagMaxParallelValue() const {
    return dag_ ? dag_->maxParallelValue() : 4;
}

QString NodeInspectorWidget::templateIdValue() const {
    return template_ ? template_->templateIdValue() : QString();
}

bool NodeInspectorWidget::buildTemplateParamsPayload(QJsonObject& outParams) {
    return template_ ? template_->buildParamsPayload(outParams) : false;
}

bool NodeInspectorWidget::buildLocalParamsPayload(QJsonObject& outParams) {
    return local_ ? local_->buildParamsPayload(outParams) : false;
}

QString NodeInspectorWidget::cronSpecValue() const {
    return actions_ ? actions_->cronSpecValue() : QString();
}

QString NodeInspectorWidget::remoteFailPolicyValue() const {
    return (remote_ && execStack_->currentWidget() == remote_) ? remote_->failPolicyValue() : QString();
}

int NodeInspectorWidget::remoteMaxParallelValue() const {
    return (remote_ && execStack_->currentWidget() == remote_) ? remote_->maxParallelValue() : 4;
}

void NodeInspectorWidget::setResultValues(const QVariantMap& result)
{
    if (runtime_) runtime_->setRuntimeValues(result);

    const QString status = result.value("status").toString();
    const qint64 duration = result.value("duration_ms").toLongLong();
    const int exitCode = result.value("exit_code").toInt();
    const QString worker = result.value("worker_id").toString();
    const int attempt = result.value("attempt").toInt();
    const int maxAttempts = result.value("max_attempts").toInt();
    const qint64 startMs = result.value("start_ts_ms").toLongLong();
    const qint64 endMs = result.value("end_ts_ms").toLongLong();

    const QString runtimeTip = tr("状态: %1\n耗时(ms): %2\n退出码: %3\nworker: %4\nstart: %5\nend: %6")
                                   .arg(status)
                                   .arg(duration)
                                   .arg(exitCode)
                                   .arg(worker)
                                   .arg(startMs)
                                   .arg(endMs);
    setToolTip(runtimeTip);
}

void NodeInspectorWidget::setReadOnlyMode(bool ro)
{
    readOnly_ = ro;
    if (common_) common_->setReadOnlyMode(ro);
    if (actions_) actions_->setReadOnlyMode(ro);
    if (local_) local_->setReadOnlyMode(ro);
    if (http_) http_->setReadOnlyMode(ro);
    if (shell_) shell_->setReadOnlyMode(ro);
    if (remote_) remote_->setReadOnlyMode(ro);
    if (dag_) dag_->setReadOnlyMode(ro);
    if (template_) template_->setReadOnlyMode(ro);
}

void NodeInspectorWidget::updateExecSection(const QString& execType)
{
    if (!common_ || !execStack_) return;

    const ExecKind kind = toExecKind(execType);
    QWidget* target = emptyExec_;

    switch (kind) {
    case ExecKind::Http:
        // common_->setCommandLabel(tr("请求 URL"));
        target = http_;
        break;
    case ExecKind::Shell:
        // common_->setCommandLabel(tr("Shell 命令"));
        target = shell_;
        break;
    case ExecKind::Remote:
        // common_->setCommandLabel(tr("远程命令/URL"));
        target = remote_;
        break;
    case ExecKind::Dag:
        // common_->setCommandLabel(tr("DAG JSON (可选)"));
        target = dag_;
        break;
    case ExecKind::Template:
        // common_->setCommandLabel(tr("模板命令/URL (可选)"));
        target = template_;
        break;
    case ExecKind::Script:
        // common_->setCommandLabel(tr("脚本/命令"));
        break;
    case ExecKind::Local:
        target = local_;
        break;
    default:
        break;
    }

    execStack_->setCurrentWidget(target);
}
