#include "node_inspector_dag_widget.h"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>

NodeInspectorDagWidget::NodeInspectorDagWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* form = new QFormLayout(this);
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(6);

    dagJsonLabel_ = new QLabel(tr("dag json"), this);
    dagJsonEdit_ = new QLineEdit(this);

    form->addRow(dagJsonLabel_, dagJsonEdit_);
}

void NodeInspectorDagWidget::setValues(const QVariantMap& exec)
{
    dagJsonEdit_->setText(exec.value("dag_json").toString());
}

QString NodeInspectorDagWidget::dagJsonValue() const { return dagJsonEdit_->text(); }

void NodeInspectorDagWidget::setReadOnlyMode(bool ro)
{
    dagJsonEdit_->setReadOnly(ro);
}
