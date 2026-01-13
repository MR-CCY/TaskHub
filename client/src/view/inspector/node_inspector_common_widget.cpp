#include "node_inspector_common_widget.h"

#include <QCheckBox>
#include <QComboBox>
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
    // cmdLabel_ = new QLabel(tr("执行脚本"), this);
    timeoutLabel_ = new QLabel(tr("超时时间"), this);
    retryLabel_ = new QLabel(tr("回退次数"), this);
    priorityLabel_ = new QLabel(tr("执行优先级"), this);
    captureLabel_ = new QLabel(tr("是否捕获日志"), this);
    descriptionLabel_ = new QLabel(tr("描述"), this);

    nameEdit_ = new QLineEdit(this);
    // cmdEdit_ = new QLineEdit(this);
    timeoutEdit_ = new QLineEdit(this);
    retryEdit_ = new QLineEdit(this);

    priorityCombo_ = new QComboBox(this);
    priorityCombo_->addItem(tr("低"), -1);
    priorityCombo_->addItem(tr("普通"), 0);
    priorityCombo_->addItem(tr("高"), 1);
    priorityCombo_->setCurrentIndex(1); // Default to Normal (0)

    captureBox_ = new QCheckBox(this);
    descriptionEdit_ = new QLineEdit(this);

    form->addRow(nameLabel_, nameEdit_);
    // form->addRow(cmdLabel_, cmdEdit_);
    form->addRow(timeoutLabel_, timeoutEdit_);
    form->addRow(retryLabel_, retryEdit_);
    form->addRow(priorityLabel_, priorityCombo_);
    form->addRow(captureLabel_, captureBox_);
    form->addRow(descriptionLabel_, descriptionEdit_);
}

void NodeInspectorCommonWidget::setValues(const QVariantMap& props)
{
    nameEdit_->setText(props.value("name").toString());
    // cmdEdit_->setText(props.value("exec_command").toString());
    timeoutEdit_->setText(QString::number(props.value("timeout_ms").toLongLong()));
    retryEdit_->setText(QString::number(props.value("retry_count").toInt()));

    int p = props.value("priority").toInt();
    int idx = priorityCombo_->findData(p);
    if (idx != -1) {
        priorityCombo_->setCurrentIndex(idx);
    } else {
        priorityCombo_->setCurrentIndex(1); // Fallback to Normal
    }

    captureBox_->setChecked(props.value("capture_output").toBool());
    descriptionEdit_->setText(props.value("description").toString());
}

QString NodeInspectorCommonWidget::nameValue() const { return nameEdit_->text(); }
qint64 NodeInspectorCommonWidget::timeoutMsValue() const { return timeoutEdit_->text().toLongLong(); }
int NodeInspectorCommonWidget::retryCountValue() const { return retryEdit_->text().toInt(); }
int NodeInspectorCommonWidget::priorityValue() const { return priorityCombo_->currentData().toInt(); }
bool NodeInspectorCommonWidget::captureOutputValue() const { return captureBox_->isChecked(); }
QString NodeInspectorCommonWidget::descriptionValue() const { return descriptionEdit_->text(); }

void NodeInspectorCommonWidget::setReadOnlyMode(bool ro)
{
    nameEdit_->setReadOnly(ro);
    timeoutEdit_->setReadOnly(ro);
    retryEdit_->setReadOnly(ro);
    priorityCombo_->setEnabled(!ro);
    captureBox_->setEnabled(!ro);
    descriptionEdit_->setReadOnly(ro);
}
