#include "node_inspector_common_widget.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>

NodeInspectorCommonWidget::NodeInspectorCommonWidget(QWidget* parent)
    : QWidget(parent)
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

    nameEdit_ = new QLineEdit(this);
    cmdEdit_ = new QLineEdit(this);
    timeoutEdit_ = new QLineEdit(this);
    retryEdit_ = new QLineEdit(this);
    queueEdit_ = new QLineEdit(this);
    captureBox_ = new QCheckBox(this);

    form->addRow(nameLabel_, nameEdit_);
    form->addRow(cmdLabel_, cmdEdit_);
    form->addRow(timeoutLabel_, timeoutEdit_);
    form->addRow(retryLabel_, retryEdit_);
    form->addRow(queueLabel_, queueEdit_);
    form->addRow(captureLabel_, captureBox_);
}

void NodeInspectorCommonWidget::setValues(const QVariantMap& props)
{
    nameEdit_->setText(props.value("name").toString());
    cmdEdit_->setText(props.value("exec_command").toString());
    timeoutEdit_->setText(QString::number(props.value("timeout_ms").toLongLong()));
    retryEdit_->setText(QString::number(props.value("retry_count").toInt()));
    queueEdit_->setText(props.value("queue").toString());
    captureBox_->setChecked(props.value("capture_output").toBool());
}

void NodeInspectorCommonWidget::setCommandLabel(const QString& text)
{
    cmdLabel_->setText(text);
}

QString NodeInspectorCommonWidget::nameValue() const { return nameEdit_->text(); }
QString NodeInspectorCommonWidget::commandValue() const { return cmdEdit_->text(); }
qint64 NodeInspectorCommonWidget::timeoutMsValue() const { return timeoutEdit_->text().toLongLong(); }
int NodeInspectorCommonWidget::retryCountValue() const { return retryEdit_->text().toInt(); }
QString NodeInspectorCommonWidget::queueValue() const { return queueEdit_->text(); }
bool NodeInspectorCommonWidget::captureOutputValue() const { return captureBox_->isChecked(); }

void NodeInspectorCommonWidget::setReadOnlyMode(bool ro)
{
    nameEdit_->setReadOnly(ro);
    cmdEdit_->setReadOnly(ro);
    timeoutEdit_->setReadOnly(ro);
    retryEdit_->setReadOnly(ro);
    queueEdit_->setReadOnly(ro);
    captureBox_->setEnabled(!ro);
}
