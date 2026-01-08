#include "node_inspector_remote_widget.h"
#include "core/app_context.h"
#include "view/worker_list_dialog.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QJsonObject>
#include <QSet>

NodeInspectorRemoteWidget::NodeInspectorRemoteWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* form = new QFormLayout(this);
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(6);

    innerTypeLabel_ = new QLabel(tr("inner exec_type"), this);
    innerCmdLabel_ = new QLabel(tr("inner exec_command"), this);
    queueLabel_ = new QLabel(tr("队列(queue)"), this);

    innerTypeEdit_ = new QLineEdit(this);
    innerCmdEdit_ = new QLineEdit(this);

    auto* qLayout = new QHBoxLayout();
    queueCombo_ = new QComboBox(this);
    queueCombo_->setEditable(true);
    
    // 提取所有队列
    QStringList queues = {"default"};
    QJsonArray workers = AppContext::instance().workers();
    QSet<QString> uniqueQueues;
    uniqueQueues.insert("default");
    for (const auto& v : workers) {
        QJsonObject w = v.toObject();
        QJsonArray qs = w.value("queues").toArray();
        for (const auto& qv : qs) {
            uniqueQueues.insert(qv.toString());
        }
    }
    queues = uniqueQueues.values();
    queues.sort();
    queueCombo_->addItems(queues);

    moreBtn_ = new QPushButton("...", this);
    moreBtn_->setFixedWidth(30);

    qLayout->addWidget(queueCombo_, 1);
    qLayout->addWidget(moreBtn_);

    form->addRow(innerTypeLabel_, innerTypeEdit_);
    form->addRow(innerCmdLabel_, innerCmdEdit_);
    form->addRow(queueLabel_, qLayout);

    connect(moreBtn_, &QPushButton::clicked, this, [this]() {
        WorkerListDialog dlg(this);
        dlg.exec();
    });
}

void NodeInspectorRemoteWidget::setValues(const QVariantMap& props, const QVariantMap& exec)
{
    innerTypeEdit_->setText(exec.value("inner.exec_type").toString());
    innerCmdEdit_->setText(exec.value("inner.exec_command").toString());

    QString q = props.value("queue").toString();
    if (q.isEmpty()) q = "default";
    queueCombo_->setCurrentText(q);
}

QString NodeInspectorRemoteWidget::innerTypeValue() const { return innerTypeEdit_->text(); }
QString NodeInspectorRemoteWidget::innerCmdValue() const { return innerCmdEdit_->text(); }
QString NodeInspectorRemoteWidget::queueValue() const { return queueCombo_->currentText(); }

void NodeInspectorRemoteWidget::setReadOnlyMode(bool ro)
{
    innerTypeEdit_->setReadOnly(ro);
    innerCmdEdit_->setReadOnly(ro);
    queueCombo_->setEnabled(!ro);
    moreBtn_->setEnabled(!ro);
}
