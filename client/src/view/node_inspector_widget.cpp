#include "node_inspector_widget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QJsonObject>
#include <QFrame>
#include <QHBoxLayout>
#include <QTimeEdit>

NodeInspectorWidget::NodeInspectorWidget(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void NodeInspectorWidget::buildUi()
{
    auto* form = new QFormLayout(this);
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(6);

    nameLabel_ = new QLabel(tr("节点名称"), this);
    cmdLabel_ = new QLabel(tr("执行脚本"), this);
    timeoutLabel_ = new QLabel(tr("超时时间"), this);
    retryLabel_ = new QLabel(tr("回退次数"), this);
    queueLabel_ = new QLabel(tr("队列分组"), this);
    captureLabel_ = new QLabel(tr("是否捕获日志"), this);
    httpMethodLabel_ = new QLabel(tr("请求方式"), this);
    httpBodyLabel_ = new QLabel(tr("请求内容"), this);
    shellCwdLabel_ = new QLabel(tr("shell cwd"), this);
    shellShellLabel_ = new QLabel(tr("shell shell"), this);
    innerTypeLabel_ = new QLabel(tr("inner exec_type"), this);
    innerCmdLabel_ = new QLabel(tr("inner exec_command"), this);
    dagJsonLabel_ = new QLabel(tr("dag json"), this);
    templateIdLabel_ = new QLabel(tr("template_id"), this);
    templateParamsLabel_ = new QLabel(tr("template_params_json"), this);

    nameEdit_ = new QLineEdit(this);
    cmdEdit_ = new QLineEdit(this);
    timeoutEdit_ = new QLineEdit(this);
    retryEdit_ = new QLineEdit(this);
    queueEdit_ = new QLineEdit(this);
    captureBox_ = new QCheckBox(this);
    httpMethodCombo_ = new QComboBox(this);
    httpMethodCombo_->setEditable(true);
    httpMethodCombo_->addItems({"GET", "POST", "PUT", "PATCH", "DELETE", "HEAD", "OPTIONS"});
    httpMethodCombo_->setMinimumWidth(140);
    httpBodyEdit_ = new QLineEdit(this);
    shellCwdEdit_ = new QLineEdit(this);
    shellShellEdit_ = new QLineEdit(this);
    innerTypeEdit_ = new QLineEdit(this);
    innerCmdEdit_ = new QLineEdit(this);
    dagJsonEdit_ = new QLineEdit(this);
    templateIdEdit_ = new QLineEdit(this);
    templateParamsEdit_ = new QLineEdit(this);
    saveBtn_ = new QPushButton(tr("保存节点"), this);
    cronTimeEdit_ = new QTimeEdit(this);
    cronTimeEdit_->setDisplayFormat("HH:mm");
    cronBtn_ = new QPushButton(tr("创建定时任务"), this);

    form->addRow(nameLabel_, nameEdit_);
    form->addRow(cmdLabel_, cmdEdit_);
    form->addRow(timeoutLabel_, timeoutEdit_);
    form->addRow(retryLabel_, retryEdit_);
    form->addRow(queueLabel_, queueEdit_);
    form->addRow(captureLabel_, captureBox_);
    form->addRow(httpMethodLabel_, httpMethodCombo_);
    form->addRow(httpBodyLabel_, httpBodyEdit_);
    form->addRow(shellCwdLabel_, shellCwdEdit_);
    form->addRow(shellShellLabel_, shellShellEdit_);
    form->addRow(innerTypeLabel_, innerTypeEdit_);
    form->addRow(innerCmdLabel_, innerCmdEdit_);
    form->addRow(dagJsonLabel_, dagJsonEdit_);
    form->addRow(templateIdLabel_, templateIdEdit_);
    form->addRow(templateParamsLabel_, templateParamsEdit_);
    form->addRow(saveBtn_);
    auto* cronRow = new QHBoxLayout();
    cronRow->setSpacing(4);
    cronRow->addWidget(cronTimeEdit_, 1);
    cronRow->addWidget(cronBtn_);
    form->addRow(tr("Cron 表达式"), cronRow);

    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    form->addRow(sep);

    runtimeStatusLabel_ = new QLabel("-", this);
    runtimeDurationLabel_ = new QLabel("-", this);
    runtimeExitLabel_ = new QLabel("-", this);
    runtimeAttemptLabel_ = new QLabel("-", this);
    runtimeWorkerLabel_ = new QLabel("-", this);
    runtimeMessageLabel_ = new QLabel("-", this);
    runtimeStdoutLabel_ = new QLabel("-", this);
    runtimeStdoutLabel_->setWordWrap(true);
    runtimeStderrLabel_ = new QLabel("-", this);
    runtimeStderrLabel_->setWordWrap(true);

    form->addRow(tr("运行状态"), runtimeStatusLabel_);
    form->addRow(tr("耗时(ms)"), runtimeDurationLabel_);
    form->addRow(tr("退出码"), runtimeExitLabel_);
    form->addRow(tr("尝试/最大"), runtimeAttemptLabel_);
    form->addRow(tr("Worker"), runtimeWorkerLabel_);
    form->addRow(tr("消息"), runtimeMessageLabel_);
    form->addRow(tr("Stdout"), runtimeStdoutLabel_);
    form->addRow(tr("Stderr"), runtimeStderrLabel_);

    connect(saveBtn_, &QPushButton::clicked, this, &NodeInspectorWidget::saveRequested);
    connect(cronBtn_, &QPushButton::clicked, this, [this]() {
        emit cronCreateRequested(cronSpecValue());
    });
}

void NodeInspectorWidget::setValues(const QVariantMap& props, const QVariantMap& exec)
{
    updateLabels(props.value("exec_type").toString());
    nameEdit_->setText(props.value("name").toString());
    cmdEdit_->setText(props.value("exec_command").toString());
    timeoutEdit_->setText(QString::number(props.value("timeout_ms").toLongLong()));
    retryEdit_->setText(QString::number(props.value("retry_count").toInt()));
    queueEdit_->setText(props.value("queue").toString());
    captureBox_->setChecked(props.value("capture_output").toBool());
    httpMethodCombo_->setCurrentText(exec.value("method").toString());
    httpBodyEdit_->setText(exec.value("body").toString());
    shellCwdEdit_->setText(exec.value("cwd").toString());
    shellShellEdit_->setText(exec.value("shell").toString());
    innerTypeEdit_->setText(exec.value("inner.exec_type").toString());
    innerCmdEdit_->setText(exec.value("inner.exec_command").toString());
    dagJsonEdit_->setText(exec.value("dag_json").toString());
    templateIdEdit_->setText(exec.value("template_id").toString());
    templateParamsEdit_->setText(exec.value("template_params_json").toString());
}

QString NodeInspectorWidget::nameValue() const { return nameEdit_->text(); }
QString NodeInspectorWidget::commandValue() const { return cmdEdit_->text(); }
qint64 NodeInspectorWidget::timeoutMsValue() const { return timeoutEdit_->text().toLongLong(); }
int NodeInspectorWidget::retryCountValue() const { return retryEdit_->text().toInt(); }
QString NodeInspectorWidget::queueValue() const { return queueEdit_->text(); }
bool NodeInspectorWidget::captureOutputValue() const { return captureBox_->isChecked(); }
QString NodeInspectorWidget::httpMethodValue() const { return httpMethodCombo_->currentText(); }
QString NodeInspectorWidget::httpBodyValue() const { return httpBodyEdit_->text(); }
QString NodeInspectorWidget::shellCwdValue() const { return shellCwdEdit_->text(); }
QString NodeInspectorWidget::shellShellValue() const { return shellShellEdit_->text(); }
QString NodeInspectorWidget::innerExecTypeValue() const { return innerTypeEdit_->text(); }
QString NodeInspectorWidget::innerExecCommandValue() const { return innerCmdEdit_->text(); }
QString NodeInspectorWidget::dagJsonValue() const { return dagJsonEdit_->text(); }
QString NodeInspectorWidget::templateIdValue() const { return templateIdEdit_->text(); }
QString NodeInspectorWidget::templateParamsJsonValue() const { return templateParamsEdit_->text(); }
QString NodeInspectorWidget::cronSpecValue() const {
    const QTime t = cronTimeEdit_->time();
    return QString("%1 %2 * * *").arg(t.minute()).arg(t.hour());
}

void NodeInspectorWidget::setRuntimeValues(const QJsonObject& obj)
{
    const QString status = obj.value("status").toString();
    const qint64 duration = obj.value("duration_ms").toVariant().toLongLong();
    const int exitCode = obj.value("exit_code").toInt();
    const QString worker = obj.value("worker_id").toString();
    const QString stdout = obj.value("stdout").toString();
    const QString stderr = obj.value("stderr").toString();
    const QString msg = obj.value("message").toString();
    const int attempt = obj.value("attempt").toInt();
    const int maxAttempts = obj.value("max_attempts").toInt();
    const qint64 startMs = obj.value("start_ts_ms").toVariant().toLongLong();
    const qint64 endMs = obj.value("end_ts_ms").toVariant().toLongLong();

    runtimeStatusLabel_->setText(status);
    runtimeDurationLabel_->setText(QString::number(duration));
    runtimeExitLabel_->setText(QString::number(exitCode));
    runtimeAttemptLabel_->setText(QString("%1 / %2").arg(attempt).arg(maxAttempts));
    runtimeWorkerLabel_->setText(worker);
    runtimeMessageLabel_->setText(msg);
    runtimeStdoutLabel_->setText(stdout);
    runtimeStderrLabel_->setText(stderr);

    // Tooltip 兼容补充
    const QString runtimeTip = tr("状态: %1\n耗时(ms): %2\n退出码: %3\nworker: %4\nstart: %5\nend: %6")
                                   .arg(status)
                                   .arg(duration)
                                   .arg(exitCode)
                                   .arg(worker)
                                   .arg(startMs)
                                   .arg(endMs);
    this->setToolTip(runtimeTip);
}

void NodeInspectorWidget::setReadOnlyMode(bool ro)
{
    readOnly_ = ro;
    nameEdit_->setReadOnly(ro);
    cmdEdit_->setReadOnly(ro);
    timeoutEdit_->setReadOnly(ro);
    retryEdit_->setReadOnly(ro);
    queueEdit_->setReadOnly(ro);
    captureBox_->setEnabled(!ro);
    httpMethodCombo_->setEnabled(!ro);
    httpBodyEdit_->setReadOnly(ro);
    shellCwdEdit_->setReadOnly(ro);
    shellShellEdit_->setReadOnly(ro);
    innerTypeEdit_->setReadOnly(ro);
    innerCmdEdit_->setReadOnly(ro);
    dagJsonEdit_->setReadOnly(ro);
    templateIdEdit_->setReadOnly(ro);
    templateParamsEdit_->setReadOnly(ro);
    saveBtn_->setVisible(!ro);
    cronTimeEdit_->setReadOnly(ro);
    cronBtn_->setEnabled(!ro);
}

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
}

void NodeInspectorWidget::updateLabels(const QString& execType)
{
    switch (toExecKind(execType)) {
    case ExecKind::Http:
        cmdLabel_->setText(tr("请求 URL"));
        httpMethodLabel_->setText(tr("HTTP 方法"));
        httpBodyLabel_->setText(tr("HTTP Body"));
        setFieldVisible(httpMethodLabel_, httpMethodCombo_, true);
        setFieldVisible(httpBodyLabel_, httpBodyEdit_, true);
        setFieldVisible(shellCwdLabel_, shellCwdEdit_, false);
        setFieldVisible(shellShellLabel_, shellShellEdit_, false);
        setFieldVisible(innerTypeLabel_, innerTypeEdit_, false);
        setFieldVisible(innerCmdLabel_, innerCmdEdit_, false);
        setFieldVisible(dagJsonLabel_, dagJsonEdit_, false);
        setFieldVisible(templateIdLabel_, templateIdEdit_, false);
        setFieldVisible(templateParamsLabel_, templateParamsEdit_, false);
        break;
    case ExecKind::Shell:
        cmdLabel_->setText(tr("Shell 命令"));
        setFieldVisible(httpMethodLabel_, httpMethodCombo_, false);
        setFieldVisible(httpBodyLabel_, httpBodyEdit_, false);
        setFieldVisible(shellCwdLabel_, shellCwdEdit_, true);
        setFieldVisible(shellShellLabel_, shellShellEdit_, true);
        setFieldVisible(innerTypeLabel_, innerTypeEdit_, false);
        setFieldVisible(innerCmdLabel_, innerCmdEdit_, false);
        setFieldVisible(dagJsonLabel_, dagJsonEdit_, false);
        setFieldVisible(templateIdLabel_, templateIdEdit_, false);
        setFieldVisible(templateParamsLabel_, templateParamsEdit_, false);
        break;
    case ExecKind::Remote:
        cmdLabel_->setText(tr("远程命令/URL"));
        setFieldVisible(httpMethodLabel_, httpMethodCombo_, false);
        setFieldVisible(httpBodyLabel_, httpBodyEdit_, false);
        setFieldVisible(shellCwdLabel_, shellCwdEdit_, false);
        setFieldVisible(shellShellLabel_, shellShellEdit_, false);
        setFieldVisible(innerTypeLabel_, innerTypeEdit_, true);
        setFieldVisible(innerCmdLabel_, innerCmdEdit_, true);
        setFieldVisible(dagJsonLabel_, dagJsonEdit_, false);
        setFieldVisible(templateIdLabel_, templateIdEdit_, false);
        setFieldVisible(templateParamsLabel_, templateParamsEdit_, false);
        break;
    case ExecKind::Dag:
        cmdLabel_->setText(tr("DAG JSON (可选)"));
        setFieldVisible(httpMethodLabel_, httpMethodCombo_, false);
        setFieldVisible(httpBodyLabel_, httpBodyEdit_, false);
        setFieldVisible(shellCwdLabel_, shellCwdEdit_, false);
        setFieldVisible(shellShellLabel_, shellShellEdit_, false);
        setFieldVisible(innerTypeLabel_, innerTypeEdit_, false);
        setFieldVisible(innerCmdLabel_, innerCmdEdit_, false);
        setFieldVisible(dagJsonLabel_, dagJsonEdit_, true);
        setFieldVisible(templateIdLabel_, templateIdEdit_, false);
        setFieldVisible(templateParamsLabel_, templateParamsEdit_, false);
        break;
    case ExecKind::Template:
        cmdLabel_->setText(tr("模板命令/URL (可选)"));
        setFieldVisible(httpMethodLabel_, httpMethodCombo_, false);
        setFieldVisible(httpBodyLabel_, httpBodyEdit_, false);
        setFieldVisible(shellCwdLabel_, shellCwdEdit_, false);
        setFieldVisible(shellShellLabel_, shellShellEdit_, false);
        setFieldVisible(innerTypeLabel_, innerTypeEdit_, false);
        setFieldVisible(innerCmdLabel_, innerCmdEdit_, false);
        setFieldVisible(dagJsonLabel_, dagJsonEdit_, false);
        setFieldVisible(templateIdLabel_, templateIdEdit_, true);
        setFieldVisible(templateParamsLabel_, templateParamsEdit_, true);
        break;
    case ExecKind::Script:
        cmdLabel_->setText(tr("脚本/命令"));
        setFieldVisible(httpMethodLabel_, httpMethodCombo_, false);
        setFieldVisible(httpBodyLabel_, httpBodyEdit_, false);
        setFieldVisible(shellCwdLabel_, shellCwdEdit_, false);
        setFieldVisible(shellShellLabel_, shellShellEdit_, false);
        setFieldVisible(innerTypeLabel_, innerTypeEdit_, false);
        setFieldVisible(innerCmdLabel_, innerCmdEdit_, false);
        setFieldVisible(dagJsonLabel_, dagJsonEdit_, false);
        setFieldVisible(templateIdLabel_, templateIdEdit_, false);
        setFieldVisible(templateParamsLabel_, templateParamsEdit_, false);
        break;
    case ExecKind::Local:
    default:
        cmdLabel_->setText(tr("处理函数/Handler"));
        setFieldVisible(httpMethodLabel_, httpMethodCombo_, false);
        setFieldVisible(httpBodyLabel_, httpBodyEdit_, false);
        setFieldVisible(shellCwdLabel_, shellCwdEdit_, false);
        setFieldVisible(shellShellLabel_, shellShellEdit_, false);
        setFieldVisible(innerTypeLabel_, innerTypeEdit_, false);
        setFieldVisible(innerCmdLabel_, innerCmdEdit_, false);
        setFieldVisible(dagJsonLabel_, dagJsonEdit_, false);
        setFieldVisible(templateIdLabel_, templateIdEdit_, false);
        setFieldVisible(templateParamsLabel_, templateParamsEdit_, false);
        break;
    }
}

void NodeInspectorWidget::setFieldVisible(QLabel* label, QWidget* editor, bool visible)
{
    label->setVisible(visible);
    editor->setVisible(visible);
}
