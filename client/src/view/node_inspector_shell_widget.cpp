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

    cmdLabel_ = new QLabel(tr("命令："), this);
    cwdLabel_ = new QLabel(tr("执行目录："), this);
    shellLabel_ = new QLabel(tr("shell："), this);
    envLabel_ = new QLabel(tr("环境变量："), this);

    cmdEdit_ = new QLineEdit(this);
    cwdEdit_ = new QLineEdit(this);
    shellEdit_ = new QLineEdit(this);
    envEdit_ = new QLineEdit(this);

    form->addRow(cmdLabel_, cmdEdit_);
    form->addRow(cwdLabel_, cwdEdit_);
    form->addRow(shellLabel_, shellEdit_);
    form->addRow(envLabel_, envEdit_);
}

void NodeInspectorShellWidget::setValues(const QVariantMap& exec)
{
    cwdEdit_->setText(exec.value("cwd").toString());
    shellEdit_->setText(exec.value("shell").toString());
    envEdit_->setText(exec.value("env").toString());
    cmdEdit_->setText(exec.value("cmd").toString());
}

QString NodeInspectorShellWidget::cwdValue() const { return cwdEdit_->text(); }
QString NodeInspectorShellWidget::shellValue() const { return shellEdit_->text(); }
QString NodeInspectorShellWidget::envValue() const { return envEdit_->text(); }
QString NodeInspectorShellWidget::cmdValue() const { return cmdEdit_->text(); }

void NodeInspectorShellWidget::setReadOnlyMode(bool ro)
{
    cwdEdit_->setReadOnly(ro);
    shellEdit_->setReadOnly(ro);
    envEdit_->setReadOnly(ro);
    cmdEdit_->setReadOnly(ro);
}
