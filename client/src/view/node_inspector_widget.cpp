#include "node_inspector_widget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

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
    saveBtn_ = new QPushButton(tr("保存节点"), this);

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
    form->addRow(saveBtn_);

    connect(saveBtn_, &QPushButton::clicked, this, &NodeInspectorWidget::saveRequested);
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

namespace {
enum class ExecKind { Local, Remote, Script, Http, Shell };
ExecKind toExecKind(const QString& s) {
    const QString t = s.trimmed().toLower();
    if (t == "httpcall" || t == "http_call" || t == "http") return ExecKind::Http;
    if (t == "shell") return ExecKind::Shell;
    if (t == "remote") return ExecKind::Remote;
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
        break;
    case ExecKind::Shell:
        cmdLabel_->setText(tr("Shell 命令"));
        setFieldVisible(httpMethodLabel_, httpMethodCombo_, false);
        setFieldVisible(httpBodyLabel_, httpBodyEdit_, false);
        setFieldVisible(shellCwdLabel_, shellCwdEdit_, true);
        setFieldVisible(shellShellLabel_, shellShellEdit_, true);
        setFieldVisible(innerTypeLabel_, innerTypeEdit_, false);
        setFieldVisible(innerCmdLabel_, innerCmdEdit_, false);
        break;
    case ExecKind::Remote:
        cmdLabel_->setText(tr("远程命令/URL"));
        setFieldVisible(httpMethodLabel_, httpMethodCombo_, false);
        setFieldVisible(httpBodyLabel_, httpBodyEdit_, false);
        setFieldVisible(shellCwdLabel_, shellCwdEdit_, false);
        setFieldVisible(shellShellLabel_, shellShellEdit_, false);
        setFieldVisible(innerTypeLabel_, innerTypeEdit_, true);
        setFieldVisible(innerCmdLabel_, innerCmdEdit_, true);
        break;
    case ExecKind::Script:
        cmdLabel_->setText(tr("脚本/命令"));
        setFieldVisible(httpMethodLabel_, httpMethodCombo_, false);
        setFieldVisible(httpBodyLabel_, httpBodyEdit_, false);
        setFieldVisible(shellCwdLabel_, shellCwdEdit_, false);
        setFieldVisible(shellShellLabel_, shellShellEdit_, false);
        setFieldVisible(innerTypeLabel_, innerTypeEdit_, false);
        setFieldVisible(innerCmdLabel_, innerCmdEdit_, false);
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
        break;
    }
}

void NodeInspectorWidget::setFieldVisible(QLabel* label, QWidget* editor, bool visible)
{
    label->setVisible(visible);
    editor->setVisible(visible);
}
