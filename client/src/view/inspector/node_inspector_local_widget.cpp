#include "node_inspector_local_widget.h"
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>

NodeInspectorLocalWidget::NodeInspectorLocalWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* form = new QFormLayout(this);
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(6);

    handlerLabel_ = new QLabel(tr("处理器 (Handler):"), this);
    handlerEdit_ = new QLineEdit(this);
    handlerEdit_->setPlaceholderText(tr("输入本地任务处理函数名..."));

    aLabel_ = new QLabel(tr("参数 A (a):"), this);
    aEdit_ = new QLineEdit(this);

    bLabel_ = new QLabel(tr("参数 B (b):"), this);
    bEdit_ = new QLineEdit(this);

    form->addRow(handlerLabel_, handlerEdit_);
    form->addRow(aLabel_, aEdit_);
    form->addRow(bLabel_, bEdit_);
}

void NodeInspectorLocalWidget::setValues(const QVariantMap& exec)
{
    handlerEdit_->setText(exec.value("handler").toString());
    aEdit_->setText(exec.value("a").toString());
    bEdit_->setText(exec.value("b").toString());
}

QString NodeInspectorLocalWidget::handlerValue() const
{
    return handlerEdit_->text();
}

QString NodeInspectorLocalWidget::aValue() const
{
    return aEdit_->text();
}

QString NodeInspectorLocalWidget::bValue() const
{
    return bEdit_->text();
}

void NodeInspectorLocalWidget::setReadOnlyMode(bool ro)
{
    handlerEdit_->setReadOnly(ro);
    aEdit_->setReadOnly(ro);
    bEdit_->setReadOnly(ro);
}
