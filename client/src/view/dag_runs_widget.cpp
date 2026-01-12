#include "dag_runs_widget.h"

#include <QDateTimeEdit>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTableView>
#include <QComboBox>
#include <QVBoxLayout>
#include <QDialog>

#include "net/api_client.h"
#include "dag_run_monitor_dialog.h"

namespace {
constexpr int kDagJsonRole = Qt::UserRole + 1;
}
DagRunsWidget::DagRunsWidget(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void DagRunsWidget::setApiClient(ApiClient* api)
{
    if (api_ == api) return;
    api_ = api;
    if (api_) {
        connect(api_, &ApiClient::dagRunsOk, this, &DagRunsWidget::onDagRuns);
    }
}

void DagRunsWidget::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    runIdEdit_ = new QLineEdit(this);
    nameEdit_ = new QLineEdit(this);
    sourceCombo_ = new QComboBox(this);
    sourceCombo_->addItem(tr("全部"), "");
    sourceCombo_->addItem(tr("手动"), "manual");
    sourceCombo_->addItem(tr("定时任务"), "cron");
    sourceCombo_->addItem(tr("嵌套执行"), "execution");
    sourceCombo_->addItem(tr("模板运行"), "template");
    sourceCombo_->addItem(tr("任务模板"), "task_template");
    sourceCombo_->addItem(tr("远程任务"), "worker");

    startEdit_ = new QDateTimeEdit(this);
    endEdit_ = new QDateTimeEdit(this);
    startEdit_->setCalendarPopup(true);
    endEdit_->setCalendarPopup(true);
    startEdit_->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    endEdit_->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    const QDateTime endDefault(QDate::currentDate(), QTime(23, 59, 59));
    const QDateTime startDefault(QDate::currentDate().addDays(-7), QTime(0, 0, 0));
    startEdit_->setDateTime(startDefault);
    endEdit_->setDateTime(endDefault);
    searchBtn_ = new QPushButton(tr("查询"), this);

    auto* filters = new QHBoxLayout();
    filters->setContentsMargins(0, 0, 0, 0);
    filters->setSpacing(8);

    filters->addWidget(new QLabel(tr("运行ID"),this));
    filters->addWidget(runIdEdit_);

    filters->addWidget(new QLabel(tr("名称"),this));
    filters->addWidget(nameEdit_);

    filters->addWidget(new QLabel(tr("来源"),this));
    filters->addWidget(sourceCombo_);

    filters->addWidget(new QLabel(tr("起止时间"),this));
    filters->addWidget(startEdit_);
    filters->addWidget(new QLabel(tr("-"),this));
    filters->addWidget(endEdit_);
    filters->addWidget(searchBtn_);
    filters->addStretch(1);

    layout->addLayout(filters);

    table_ = new QTableView(this);
    model_ = new QStandardItemModel(this);
    QStringList headers = {
        tr("运行ID"), tr("名称"), tr("来源"), tr("状态"),
        tr("开始时间"), tr("结束时间"),
        tr("总数"), tr("成功"), tr("失败"), tr("跳过"),
        tr("消息")
    };
    model_->setHorizontalHeaderLabels(headers);
    table_->verticalHeader()->setDefaultAlignment(Qt::AlignCenter);
    table_->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->verticalHeader()->setMinimumSectionSize(24);
    table_->verticalHeader()->setVisible(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setModel(model_);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->setSortingEnabled(true);
    table_->sortByColumn(4, Qt::DescendingOrder); // start time desc
    layout->addWidget(table_);

    connect(searchBtn_, &QPushButton::clicked, this, &DagRunsWidget::onSearch);
    connect(table_, &QTableView::doubleClicked, this, &DagRunsWidget::onRowDoubleClicked);
}

void DagRunsWidget::onSearch()
{
    if (!api_) return;
    const QString runId = runIdEdit_->text().trimmed();
    const QString name = nameEdit_->text().trimmed();
    const QString source = sourceCombo_->currentData().toString();
    const qint64 startMs = startEdit_->dateTime().isValid() ? startEdit_->dateTime().toMSecsSinceEpoch() : 0;
    const qint64 endMs = endEdit_->dateTime().isValid() ? endEdit_->dateTime().toMSecsSinceEpoch() : 0;
    api_->getDagRuns(runId, name, source, startMs, endMs, 100);
}

QString DagRunsWidget::formatTs(qint64 ms) const
{
    if (ms <= 0) return "-";
    return QDateTime::fromMSecsSinceEpoch(ms).toString("yyyy-MM-dd HH:mm:ss");
}

void DagRunsWidget::fillRow(int row, const QJsonObject& obj)
{
    const QString runId = obj.value("run_id").toString();
    const QString name = obj.value("name").toString();
    const QString source = obj.value("source").toString();
    const QString status = obj.value("status").toString();
    const qint64 startMs = obj.value("start_ts_ms").toVariant().toLongLong();
    const qint64 endMs = obj.value("end_ts_ms").toVariant().toLongLong();
    const int total = obj.value("total").toInt();
    const int succ = obj.value("success_count").toInt();
    const int fail = obj.value("failed_count").toInt();
    const int skip = obj.value("skipped_count").toInt();
    const QString message = obj.value("message").toString();
    const QString dag_json= obj.value("dag_json").toString();

    QString sourceText = source;
    if (source == "manual") sourceText = tr("手动");
    else if (source == "cron") sourceText = tr("定时任务");
    else if (source == "execution") sourceText = tr("嵌套执行");
    else if (source == "template") sourceText = tr("模板运行");
    else if (source == "task_template") sourceText = tr("任务模板");
    else if (source == "worker") sourceText = tr("远程任务");

    QList<QStandardItem*> items;
    auto* runItem = new QStandardItem(runId);
    runItem->setData(dag_json, kDagJsonRole);
    items << runItem;
    items << new QStandardItem(name);
    items << new QStandardItem(sourceText);
    items << new QStandardItem(status);
    auto* startIt = new QStandardItem(formatTs(startMs));
    startIt->setData(startMs, Qt::UserRole);
    items << startIt;
    items << new QStandardItem(formatTs(endMs));
    items << new QStandardItem(QString::number(total));
    items << new QStandardItem(QString::number(succ));
    items << new QStandardItem(QString::number(fail));
    items << new QStandardItem(QString::number(skip));
    items << new QStandardItem(message);
    for (auto* it : items) it->setEditable(false);
    model_->appendRow(items);
}

void DagRunsWidget::onDagRuns(const QJsonArray& items)
{
    model_->removeRows(0, model_->rowCount());
    for (const auto& val : items) {
        if (!val.isObject()) continue;
        fillRow(model_->rowCount(), val.toObject());
    }
    table_->sortByColumn(4, Qt::DescendingOrder);
}

void DagRunsWidget::onRowDoubleClicked(const QModelIndex& index)
{
    if (!index.isValid()) return;
    const int row = index.row();
    const QString dagJsonStr = model_->index(row, 0).data(kDagJsonRole).toString();
    if (dagJsonStr.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("当前行没有 dag_json 数据"));
        return;
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(dagJsonStr.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::warning(this, tr("解析失败"), tr("dag_json 解析失败: %1").arg(err.errorString()));
        return;
    }

    auto* dlg = new DagRunMonitorDialog(model_->index(row, 0).data().toString(),
                                        dagJsonStr,
                                        api_,
                                        this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}
