#include "node_inspector_action_widget.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTimeEdit>

NodeInspectorActionWidget::NodeInspectorActionWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* form = new QFormLayout(this);
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(6);

    saveBtn_ = new QPushButton(tr("保存节点"), this);
    cronTimeEdit_ = new QTimeEdit(this);
    cronTimeEdit_->setDisplayFormat("HH:mm");
    cronBtn_ = new QPushButton(tr("创建定时任务"), this);

    auto* cronRowWidget = new QWidget(this);
    auto* cronRow = new QHBoxLayout(cronRowWidget);
    cronRow->setContentsMargins(0, 0, 0, 0);
    cronRow->setSpacing(4);
    cronRow->addWidget(cronTimeEdit_, 1);
    cronRow->addWidget(cronBtn_);

    form->addRow(saveBtn_);
    form->addRow(tr("Cron 表达式"), cronRowWidget);
}

QPushButton* NodeInspectorActionWidget::saveButton() const { return saveBtn_; }
QPushButton* NodeInspectorActionWidget::cronButton() const { return cronBtn_; }

QString NodeInspectorActionWidget::cronSpecValue() const
{
    const QTime t = cronTimeEdit_->time();
    return QString("%1 %2 * * *").arg(t.minute()).arg(t.hour());
}

void NodeInspectorActionWidget::setReadOnlyMode(bool ro)
{
    saveBtn_->setVisible(!ro);
    cronTimeEdit_->setReadOnly(ro);
    cronBtn_->setEnabled(!ro);
}
