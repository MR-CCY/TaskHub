#include "node_inspector_shell_widget.h"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>

NodeInspectorShellWidget::NodeInspectorShellWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* form = new QFormLayout(this);
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(6);

    cwdLabel_ = new QLabel(tr("shell cwd"), this);
    shellLabel_ = new QLabel(tr("shell shell"), this);
    cwdEdit_ = new QLineEdit(this);
    shellEdit_ = new QLineEdit(this);

    form->addRow(cwdLabel_, cwdEdit_);
    form->addRow(shellLabel_, shellEdit_);
}

void NodeInspectorShellWidget::setValues(const QVariantMap& exec)
{
    cwdEdit_->setText(exec.value("cwd").toString());
    shellEdit_->setText(exec.value("shell").toString());
}

QString NodeInspectorShellWidget::cwdValue() const { return cwdEdit_->text(); }
QString NodeInspectorShellWidget::shellValue() const { return shellEdit_->text(); }

void NodeInspectorShellWidget::setReadOnlyMode(bool ro)
{
    cwdEdit_->setReadOnly(ro);
    shellEdit_->setReadOnly(ro);
}
