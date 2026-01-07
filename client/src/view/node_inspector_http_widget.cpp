#include "node_inspector_http_widget.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>

NodeInspectorHttpWidget::NodeInspectorHttpWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* form = new QFormLayout(this);
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(6);

    methodLabel_ = new QLabel(tr("请求方式"), this);
    bodyLabel_ = new QLabel(tr("请求内容"), this);

    methodCombo_ = new QComboBox(this);
    methodCombo_->setEditable(true);
    methodCombo_->addItems({"GET", "POST", "PUT", "PATCH", "DELETE", "HEAD", "OPTIONS"});
    methodCombo_->setMinimumWidth(140);

    bodyEdit_ = new QLineEdit(this);

    form->addRow(methodLabel_, methodCombo_);
    form->addRow(bodyLabel_, bodyEdit_);
}

void NodeInspectorHttpWidget::setValues(const QVariantMap& exec)
{
    methodCombo_->setCurrentText(exec.value("method").toString());
    bodyEdit_->setText(exec.value("body").toString());
}

QString NodeInspectorHttpWidget::methodValue() const { return methodCombo_->currentText(); }
QString NodeInspectorHttpWidget::bodyValue() const { return bodyEdit_->text(); }

void NodeInspectorHttpWidget::setReadOnlyMode(bool ro)
{
    methodCombo_->setEnabled(!ro);
    bodyEdit_->setReadOnly(ro);
}
