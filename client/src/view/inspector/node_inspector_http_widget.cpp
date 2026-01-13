#include "node_inspector_http_widget.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QEvent>
#include <QMouseEvent>
#include "headers_editor_dialog.h"
#include "body_editor_dialog.h"

NodeInspectorHttpWidget::NodeInspectorHttpWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* form = new QFormLayout(this);
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(6);

    urlLabel_ = new QLabel(tr("请求地址:"), this);
    methodLabel_ = new QLabel(tr("请求方式:"), this);
    headersLabel_ = new QLabel(tr("请求头:"), this);
    bodyLabel_ = new QLabel(tr("请求内容:"), this);
    authUserLabel_ = new QLabel(tr("Basic Auth 用户:"), this);
    authPassLabel_ = new QLabel(tr("Basic Auth 密码:"), this);
    followLabel_ = new QLabel(tr("自动重定向:"), this);

    urlEdit_ = new QLineEdit(this);
    methodCombo_ = new QComboBox(this);
    methodCombo_->setEditable(true);
    methodCombo_->addItems({"GET", "POST", "PUT", "PATCH", "DELETE", "HEAD", "OPTIONS"});
    methodCombo_->setMinimumWidth(140);

    headersEdit_ = new QLineEdit(this);
    headersEdit_->setReadOnly(true);
    headersEdit_->setPlaceholderText(tr("双击编辑 Headers..."));
    headersEdit_->installEventFilter(this);

    bodyEdit_ = new QLineEdit(this);
    bodyEdit_->setReadOnly(true);
    bodyEdit_->setPlaceholderText(tr("双击编辑 Body..."));
    bodyEdit_->installEventFilter(this);

    authUserEdit_ = new QLineEdit(this);
    authPassEdit_ = new QLineEdit(this);

    followCheck_ = new QCheckBox(this);
    followCheck_->setChecked(true);

    form->addRow(urlLabel_, urlEdit_);
    form->addRow(methodLabel_, methodCombo_);
    form->addRow(headersLabel_, headersEdit_);
    form->addRow(bodyLabel_, bodyEdit_);
    form->addRow(authUserLabel_, authUserEdit_);
    form->addRow(authPassLabel_, authPassEdit_);
    form->addRow(followLabel_, followCheck_);
}

void NodeInspectorHttpWidget::setValues(const QVariantMap& exec)
{
    urlEdit_->setText(exec.value("url").toString());
    methodCombo_->setCurrentText(exec.value("method", "GET").toString());
    
    fullBody_ = exec.value("body").toString();
    bodyEdit_->setText(fullBody_.isEmpty() ? "" : tr("(数据已输入)"));

    authUserEdit_->setText(exec.value("auth.user").toString());
    authPassEdit_->setText(exec.value("auth.pass").toString());
    
    QString follow = exec.value("follow_redirects", "true").toString();
    followCheck_->setChecked(follow == "true" || follow == "1");

    // 解析并合并 Headers
    QStringList headerLines;
    for (auto it = exec.begin(); it != exec.end(); ++it) {
        if (it.key().startsWith("header.")) {
            headerLines << QString("%1: %2").arg(it.key().mid(7)).arg(it.value().toString());
        }
    }
    fullHeaders_ = headerLines.join("\n");
    headersEdit_->setText(fullHeaders_.isEmpty() ? "" : tr("(已设置 %1 条)").arg(headerLines.size()));
}

QString NodeInspectorHttpWidget::urlValue() const { return urlEdit_->text(); }
QString NodeInspectorHttpWidget::methodValue() const { return methodCombo_->currentText(); }
QString NodeInspectorHttpWidget::headersValue() const { return fullHeaders_; }
QString NodeInspectorHttpWidget::bodyValue() const { return fullBody_; }
QString NodeInspectorHttpWidget::authUserValue() const { return authUserEdit_->text(); }
QString NodeInspectorHttpWidget::authPassValue() const { return authPassEdit_->text(); }
bool NodeInspectorHttpWidget::followRedirectsValue() const { return followCheck_->isChecked(); }

bool NodeInspectorHttpWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonDblClick) {
        bool ro = urlEdit_->isReadOnly();
        
        if (obj == headersEdit_) {
            HeadersEditorDialog dlg(fullHeaders_, ro, this);
            if (dlg.exec() == QDialog::Accepted) {
                fullHeaders_ = dlg.getHeadersText();
                int count = fullHeaders_.split("\n", Qt::SkipEmptyParts).size();
                headersEdit_->setText(fullHeaders_.isEmpty() ? "" : tr("(已设置 %1 条)").arg(count));
            }
            return true;
        } else if (obj == bodyEdit_) {
            BodyEditorDialog dlg(fullBody_, ro, this);
            if (dlg.exec() == QDialog::Accepted) {
                fullBody_ = dlg.getBodyText();
                bodyEdit_->setText(fullBody_.isEmpty() ? "" : tr("(数据已输入)"));
            }
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void NodeInspectorHttpWidget::setReadOnlyMode(bool ro)
{
    urlEdit_->setReadOnly(ro);
    methodCombo_->setEnabled(!ro);
    headersEdit_->setReadOnly(ro);
    bodyEdit_->setReadOnly(ro);
    authUserEdit_->setReadOnly(ro);
    authPassEdit_->setReadOnly(ro);
    followCheck_->setEnabled(!ro);
}
