#include "node_inspector_dag_widget.h"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>

NodeInspectorDagWidget::NodeInspectorDagWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* form = new QFormLayout(this);
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(6);

    failPolicyLabel_ = new QLabel(tr("失败策略"), this);
    failPolicyCombo_ = new QComboBox(this);
    failPolicyCombo_->addItem(tr("失败即终止"), QStringLiteral("FailFast"));
    failPolicyCombo_->addItem(tr("失败则跳过"), QStringLiteral("SkipDownstream"));

    maxParallelLabel_ = new QLabel(tr("并发限制"), this);
    maxParallelSpin_ = new QSpinBox(this);
    maxParallelSpin_->setRange(1, 128);
    maxParallelSpin_->setValue(4);

    form->addRow(failPolicyLabel_, failPolicyCombo_);
    form->addRow(maxParallelLabel_, maxParallelSpin_);
}

void NodeInspectorDagWidget::setValues(const QVariantMap& exec)
{
    QString fp = exec.value("config.fail_policy").toString().trimmed();
    int idx = failPolicyCombo_->findData(fp);
    if (idx >= 0) failPolicyCombo_->setCurrentIndex(idx);
    else failPolicyCombo_->setCurrentIndex(0);

    int mp = exec.value("config.max_parallel", 4).toInt();
    maxParallelSpin_->setValue(mp);
}

QString NodeInspectorDagWidget::failPolicyValue() const { return failPolicyCombo_->currentData().toString(); }
int NodeInspectorDagWidget::maxParallelValue() const { return maxParallelSpin_->value(); }

void NodeInspectorDagWidget::setReadOnlyMode(bool ro)
{
    failPolicyCombo_->setEnabled(!ro);
    maxParallelSpin_->setEnabled(!ro);
}
