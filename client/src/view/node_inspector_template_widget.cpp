#include "node_inspector_template_widget.h"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>

NodeInspectorTemplateWidget::NodeInspectorTemplateWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* form = new QFormLayout(this);
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(6);

    templateIdLabel_ = new QLabel(tr("template_id"), this);
    templateParamsLabel_ = new QLabel(tr("template_params_json"), this);
    templateIdEdit_ = new QLineEdit(this);
    templateParamsEdit_ = new QLineEdit(this);

    form->addRow(templateIdLabel_, templateIdEdit_);
    form->addRow(templateParamsLabel_, templateParamsEdit_);
}

void NodeInspectorTemplateWidget::setValues(const QVariantMap& exec)
{
    templateIdEdit_->setText(exec.value("template_id").toString());
    templateParamsEdit_->setText(exec.value("template_params_json").toString());
}

QString NodeInspectorTemplateWidget::templateIdValue() const { return templateIdEdit_->text(); }
QString NodeInspectorTemplateWidget::templateParamsValue() const { return templateParamsEdit_->text(); }

void NodeInspectorTemplateWidget::setReadOnlyMode(bool ro)
{
    templateIdEdit_->setReadOnly(ro);
    templateParamsEdit_->setReadOnly(ro);
}
