#include "node_inspector_remote_widget.h"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>

NodeInspectorRemoteWidget::NodeInspectorRemoteWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* form = new QFormLayout(this);
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(6);

    innerTypeLabel_ = new QLabel(tr("inner exec_type"), this);
    innerCmdLabel_ = new QLabel(tr("inner exec_command"), this);
    innerTypeEdit_ = new QLineEdit(this);
    innerCmdEdit_ = new QLineEdit(this);

    form->addRow(innerTypeLabel_, innerTypeEdit_);
    form->addRow(innerCmdLabel_, innerCmdEdit_);
}

void NodeInspectorRemoteWidget::setValues(const QVariantMap& exec)
{
    innerTypeEdit_->setText(exec.value("inner.exec_type").toString());
    innerCmdEdit_->setText(exec.value("inner.exec_command").toString());
}

QString NodeInspectorRemoteWidget::innerTypeValue() const { return innerTypeEdit_->text(); }
QString NodeInspectorRemoteWidget::innerCmdValue() const { return innerCmdEdit_->text(); }

void NodeInspectorRemoteWidget::setReadOnlyMode(bool ro)
{
    innerTypeEdit_->setReadOnly(ro);
    innerCmdEdit_->setReadOnly(ro);
}
