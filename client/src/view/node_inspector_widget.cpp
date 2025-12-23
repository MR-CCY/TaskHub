#include "node_inspector_widget.h"

#include <QCheckBox>
#include <QFormLayout>
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

    nameEdit_ = new QLineEdit(this);
    cmdEdit_ = new QLineEdit(this);
    timeoutEdit_ = new QLineEdit(this);
    retryEdit_ = new QLineEdit(this);
    queueEdit_ = new QLineEdit(this);
    captureBox_ = new QCheckBox(this);
    httpMethodEdit_ = new QLineEdit(this);
    httpBodyEdit_ = new QLineEdit(this);
    shellCwdEdit_ = new QLineEdit(this);
    shellShellEdit_ = new QLineEdit(this);
    innerTypeEdit_ = new QLineEdit(this);
    innerCmdEdit_ = new QLineEdit(this);
    saveBtn_ = new QPushButton(tr("保存节点"), this);

    form->addRow(tr("节点名称："), nameEdit_);
    form->addRow(tr("执行脚本："), cmdEdit_);
    form->addRow(tr("超时时间："), timeoutEdit_);
    form->addRow(tr("回退次数："), retryEdit_);
    form->addRow(tr("队列分组"), queueEdit_);
    form->addRow(tr("是否捕获日志"), captureBox_);
    form->addRow(tr("请求方式"), httpMethodEdit_);
    form->addRow(tr("请求内容"), httpBodyEdit_);
    form->addRow(tr("shell cwd"), shellCwdEdit_);
    form->addRow(tr("shell shell"), shellShellEdit_);
    form->addRow(tr("inner exec_type"), innerTypeEdit_);
    form->addRow(tr("inner exec_command"), innerCmdEdit_);
    form->addRow(saveBtn_);

    connect(saveBtn_, &QPushButton::clicked, this, &NodeInspectorWidget::saveRequested);
}

void NodeInspectorWidget::setValues(const QVariantMap& props, const QVariantMap& exec)
{
    nameEdit_->setText(props.value("name").toString());
    cmdEdit_->setText(props.value("exec_command").toString());
    timeoutEdit_->setText(QString::number(props.value("timeout_ms").toLongLong()));
    retryEdit_->setText(QString::number(props.value("retry_count").toInt()));
    queueEdit_->setText(props.value("queue").toString());
    captureBox_->setChecked(props.value("capture_output").toBool());
    httpMethodEdit_->setText(exec.value("method").toString());
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
QString NodeInspectorWidget::httpMethodValue() const { return httpMethodEdit_->text(); }
QString NodeInspectorWidget::httpBodyValue() const { return httpBodyEdit_->text(); }
QString NodeInspectorWidget::shellCwdValue() const { return shellCwdEdit_->text(); }
QString NodeInspectorWidget::shellShellValue() const { return shellShellEdit_->text(); }
QString NodeInspectorWidget::innerExecTypeValue() const { return innerTypeEdit_->text(); }
QString NodeInspectorWidget::innerExecCommandValue() const { return innerCmdEdit_->text(); }
