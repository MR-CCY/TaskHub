#include "dag_inspector_widget.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QHBoxLayout>
#include <QTimeEdit>

DagInspectorWidget::DagInspectorWidget(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void DagInspectorWidget::buildUi()
{
    auto* form = new QFormLayout(this);
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(6);

    nameEdit_ = new QLineEdit(this);
    failPolicyCombo_ = new QComboBox(this);
    failPolicyCombo_->addItem(tr("失败即终止"), QStringLiteral("FailFast"));
    failPolicyCombo_->addItem(tr("失败则跳过"), QStringLiteral("SkipDownstream"));
    maxParallelSpin_ = new QSpinBox(this);
    maxParallelSpin_->setRange(1, 128);
    saveBtn_ = new QPushButton(tr("保存 DAG"), this);
    runBtn_ = new QPushButton(tr("运行 DAG"), this);
    cronTimeEdit_ = new QTimeEdit(this);
    cronTimeEdit_->setDisplayFormat("HH:mm");
    cronBtn_ = new QPushButton(tr("创建定时任务"), this);

    form->addRow(tr("DAG名称："), nameEdit_);
    form->addRow(tr("失败策略："), failPolicyCombo_);
    form->addRow(tr("最大并行任务："), maxParallelSpin_);
    form->addRow(saveBtn_);
    form->addRow(runBtn_);
    auto* cronRow = new QHBoxLayout();
    cronRow->setSpacing(4);
    cronRow->addWidget(cronTimeEdit_, 1);
    cronRow->addWidget(cronBtn_);
    form->addRow(tr("Cron 表达式"), cronRow);

    connect(saveBtn_, &QPushButton::clicked, this, &DagInspectorWidget::saveRequested);
    connect(runBtn_, &QPushButton::clicked, this, &DagInspectorWidget::runRequested);
    connect(cronBtn_, &QPushButton::clicked, this, [this]() {
        emit cronCreateRequested(cronSpecValue());
    });
}

void DagInspectorWidget::setValues(const QString& name, const QString& failPolicy, int maxParallel)
{
    nameEdit_->setText(name);
    int idx = failPolicyCombo_->findData(failPolicy);
    if (idx < 0) idx = failPolicyCombo_->findText(failPolicy);
    if (idx < 0) idx = 0;
    failPolicyCombo_->setCurrentIndex(idx);
    maxParallelSpin_->setValue(maxParallel);
}

QString DagInspectorWidget::nameValue() const { return nameEdit_->text(); }
QString DagInspectorWidget::failPolicyValue() const {
    const QVariant v = failPolicyCombo_->currentData();
    return v.isValid() ? v.toString() : failPolicyCombo_->currentText();
}
int DagInspectorWidget::maxParallelValue() const { return maxParallelSpin_->value(); }
QString DagInspectorWidget::cronSpecValue() const {
    const QTime t = cronTimeEdit_->time();
    // Daily schedule HH:mm -> "<min> <hour> * * *"
    return QString("%1 %2 * * *").arg(t.minute()).arg(t.hour());
}
