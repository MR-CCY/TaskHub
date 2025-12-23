#include "dag_inspector_widget.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

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
    failPolicyCombo_->addItem("失败即终止");
    failPolicyCombo_->addItem("失败则跳过");
    maxParallelSpin_ = new QSpinBox(this);
    maxParallelSpin_->setRange(1, 128);
    saveBtn_ = new QPushButton(tr("保存 DAG"), this);

    form->addRow(tr("DAG名称："), nameEdit_);
    form->addRow(tr("失败策略："), failPolicyCombo_);
    form->addRow(tr("最大并行任务："), maxParallelSpin_);
    form->addRow(saveBtn_);

    connect(saveBtn_, &QPushButton::clicked, this, &DagInspectorWidget::saveRequested);
}

void DagInspectorWidget::setValues(const QString& name, const QString& failPolicy, int maxParallel)
{
    nameEdit_->setText(name);
    failPolicyCombo_->setCurrentText(failPolicy);
    maxParallelSpin_->setValue(maxParallel);
}

QString DagInspectorWidget::nameValue() const { return nameEdit_->text(); }
QString DagInspectorWidget::failPolicyValue() const { return failPolicyCombo_->currentText(); }
int DagInspectorWidget::maxParallelValue() const { return maxParallelSpin_->value(); }
