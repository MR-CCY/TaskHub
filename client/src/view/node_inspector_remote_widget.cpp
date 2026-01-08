#include "node_inspector_remote_widget.h"
#include "core/app_context.h"
#include "view/worker_list_dialog.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QSpinBox>
#include <QJsonObject>
#include <QSet>

NodeInspectorRemoteWidget::NodeInspectorRemoteWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* form = new QFormLayout(this);
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(6);

    queueLabel_ = new QLabel(tr("队列(queue)"), this);
    failPolicyLabel_ = new QLabel(tr("失败策略"), this);
    maxParallelLabel_ = new QLabel(tr("并发限制"), this);

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

    failPolicyCombo_ = new QComboBox(this);
    failPolicyCombo_->addItem(tr("失败即终止"), QStringLiteral("FailFast"));
    failPolicyCombo_->addItem(tr("失败则跳过"), QStringLiteral("SkipDownstream"));

    maxParallelSpin_ = new QSpinBox(this);
    maxParallelSpin_->setRange(1, 128);
    maxParallelSpin_->setValue(4);

    form->addRow(queueLabel_, qLayout);
    form->addRow(failPolicyLabel_, failPolicyCombo_);
    form->addRow(maxParallelLabel_, maxParallelSpin_);

    connect(moreBtn_, &QPushButton::clicked, this, [this]() {
        WorkerListDialog dlg(this);
        dlg.exec();
    });
}

void NodeInspectorRemoteWidget::setValues(const QVariantMap& props, const QVariantMap& exec)
{
    QString q = props.value("queue").toString();
    if (q.isEmpty()) q = "default";
    queueCombo_->setCurrentText(q);

    QString fp = exec.value("remote.fail_policy").toString().trimmed();
    int idx = failPolicyCombo_->findData(fp);
    if (idx >= 0) failPolicyCombo_->setCurrentIndex(idx);
    else failPolicyCombo_->setCurrentIndex(0);

    int mp = exec.value("remote.max_parallel", 4).toInt();
    maxParallelSpin_->setValue(mp);
}

QString NodeInspectorRemoteWidget::queueValue() const { return queueCombo_->currentText(); }
QString NodeInspectorRemoteWidget::failPolicyValue() const { return failPolicyCombo_->currentData().toString(); }
int NodeInspectorRemoteWidget::maxParallelValue() const { return maxParallelSpin_->value(); }

void NodeInspectorRemoteWidget::setReadOnlyMode(bool ro)
{
    queueCombo_->setEnabled(!ro);
    moreBtn_->setEnabled(!ro);
    failPolicyCombo_->setEnabled(!ro);
    maxParallelSpin_->setEnabled(!ro);
}
