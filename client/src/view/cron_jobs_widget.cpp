#include "cron_jobs_widget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTableView>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QMessageBox>
#include <QDateTime>
#include <QAbstractItemView>

#include "net/api_client.h"

CronJobsWidget::CronJobsWidget(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void CronJobsWidget::buildUi()
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    auto* btns = new QHBoxLayout();
    btns->setSpacing(6);
    refreshBtn_ = new QPushButton(tr("刷新"), this);
    deleteBtn_ = new QPushButton(tr("删除"), this);
    btns->addWidget(refreshBtn_);
    btns->addWidget(deleteBtn_);
    btns->addStretch();
    lay->addLayout(btns);

    model_ = new QStandardItemModel(0, 5, this);
    model_->setHorizontalHeaderLabels({tr("ID"), tr("名称"), tr("Spec"), tr("类型"), tr("下次时间")});

    table_ = new QTableView(this);
    table_->setModel(model_);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    lay->addWidget(table_, 1);

    connect(refreshBtn_, &QPushButton::clicked, this, &CronJobsWidget::onRefresh);
    connect(deleteBtn_, &QPushButton::clicked, this, &CronJobsWidget::onDelete);
}

void CronJobsWidget::setApiClient(ApiClient* api)
{
    api_ = api;
    if (!api_) return;
    connect(api_, &ApiClient::cronJobsOk, this, &CronJobsWidget::onCronJobs, Qt::UniqueConnection);
    connect(api_, &ApiClient::cronJobDeleted, this, &CronJobsWidget::onCronChanged, Qt::UniqueConnection);
    connect(api_, &ApiClient::cronJobCreated, this, &CronJobsWidget::onCronChanged, Qt::UniqueConnection);
    onRefresh();
}

void CronJobsWidget::onRefresh()
{
    if (api_) api_->listCronJobs();
}

void CronJobsWidget::onDelete()
{
    if (!api_) return;
    const QString id = selectedId();
    if (id.isEmpty()) return;
    api_->deleteCronJob(id);
}

QString CronJobsWidget::selectedId() const
{
    const auto sel = table_->selectionModel()->selectedRows();
    if (sel.isEmpty()) return {};
    const int row = sel.first().row();
    return model_->item(row, 0) ? model_->item(row, 0)->text() : QString();
}

void CronJobsWidget::onCronJobs(const QJsonArray& items)
{
    model_->setRowCount(items.size());
    int row = 0;
    for (const auto& v : items) {
        if (!v.isObject()) continue;
        fillRow(row++, v.toObject());
    }
}

void CronJobsWidget::fillRow(int row, const QJsonObject& obj)
{
    const QString id = obj.value("id").toString();
    const QString name = obj.value("name").toString();
    const QString spec = obj.value("spec").toString();
    const QString target = obj.value("target_type").toString();
    const qint64 nextEpoch = obj.value("next_time_epoch").toVariant().toLongLong();
    const QString nextStr = nextEpoch > 0 ? QDateTime::fromSecsSinceEpoch(nextEpoch).toString("yyyy-MM-dd HH:mm:ss") : "-";
    const QStringList cols = {id, name, spec, target, nextStr};
    for (int c = 0; c < cols.size(); ++c) {
        model_->setItem(row, c, new QStandardItem(cols[c]));
    }
}

void CronJobsWidget::onCronChanged()
{
    onRefresh();
}
