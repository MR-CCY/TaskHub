#include "dag_run_monitor_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTableWidget>
#include <QHeaderView>
#include <QListWidget>
#include <QTabWidget>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QLabel>
#include <QTimer>

#include "dag_run_viewer.h"
#include "net/api_client.h"
#include "view/inspector_panel.h"

namespace {
QJsonObject parseJson(const QByteArray& bytes) {
    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(bytes, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return {};
    return doc.object();
}
}

DagRunMonitorDialog::DagRunMonitorDialog(const QString& runId,
                                         const QString& dagJson,
                                         ApiClient* api,
                                         QWidget* parent)
    : QDialog(parent),
      runId_(runId),
      dagJson_(dagJson),
      api_(api)
{
    setModal(true);
    setWindowTitle(tr("DAG 监控 - %1").arg(runId_));
    resize(1200, 800);
    buildUi();

    pollTimer_.setInterval(500);
    connect(&pollTimer_, &QTimer::timeout, this, &DagRunMonitorDialog::onTimelineTick);
    pollTimer_.start();

    fetchTaskRuns();
    fetchEvents();
}

DagRunMonitorDialog::~DagRunMonitorDialog() = default;

void DagRunMonitorDialog::buildUi()
{
    auto* mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(8, 8, 8, 8);
    mainLay->setSpacing(6);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    bench_ = new DagRunViewerBench(this);
    const QJsonDocument doc = QJsonDocument::fromJson(dagJson_.toUtf8());
    if (doc.isObject()) {
        bench_->loadDagJson(doc.object());
    }
    splitter->addWidget(bench_);

    inspector_ = new InspectorPanel(bench_->canvasScene(), bench_->benchUndo(), bench_->benchView(), this);
    inspector_->setApiClient(api_);
    inspector_->setReadOnlyMode(true);
    splitter->addWidget(inspector_);

    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    mainLay->addWidget(splitter, 5);

    tabs_ = new QTabWidget(this);
    timelineList_ = new QListWidget(this);
    logList_ = new QListWidget(this);
    wsRawList_ = new QListWidget(this);
    tabs_->addTab(timelineList_, tr("Timeline"));
    tabs_->addTab(logList_, tr("Task Logs"));
    tabs_->addTab(wsRawList_, tr("Raw WS Events"));
    mainLay->addWidget(tabs_, 2);
}

void DagRunMonitorDialog::fetchTaskRuns()
{
    if (!api_) return;
    QUrl url(api_->baseUrl() + "/api/dag/task_runs");
    QUrlQuery q;
    q.addQueryItem("run_id", runId_);
    q.addQueryItem("limit", "500");
    url.setQuery(q);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!api_->token().isEmpty()) {
        req.setRawHeader("Authorization", "Bearer " + api_->token().toUtf8());
    }
    auto* nam = new QNetworkAccessManager(this);
    auto* reply = nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const auto obj = parseJson(reply->readAll());
        reply->deleteLater();
        auto items = obj.value("data").toObject().value("items").toArray();
        populateTaskRuns(items);
    });
}

void DagRunMonitorDialog::populateTaskRuns(const QJsonArray& items)
{
    int idx = 0;
    for (const auto& v : items) {
        const auto o = v.toObject();
        const QString logical = o.value("logical_id").toString();
        const int status = o.value("status").toInt();
        bench_->setNodeStatus(logical, statusColor(status));
        bench_->setNodeStatusLabel(logical, statusText(status));
        if (idx == 0) {
            bench_->selectNode(logical);
        }
        ++idx;
    }
    if (!items.isEmpty()) {
        inspector_->setTaskRuns(items);
    }
}

void DagRunMonitorDialog::fetchEvents()
{
    if (!api_) return;
    QUrl url(api_->baseUrl() + "/api/dag/events");
    QUrlQuery q;
    q.addQueryItem("run_id", runId_);
    if (lastEventTs_ > 0) q.addQueryItem("start_ts_ms", QString::number(lastEventTs_ + 1));
    q.addQueryItem("limit", "200");
    url.setQuery(q);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!api_->token().isEmpty()) {
        req.setRawHeader("Authorization", "Bearer " + api_->token().toUtf8());
    }
    auto* nam = new QNetworkAccessManager(this);
    auto* reply = nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const auto obj = parseJson(reply->readAll());
        reply->deleteLater();
        auto items = obj.value("data").toObject().value("items").toArray();
        appendEvents(items);
    });
}

void DagRunMonitorDialog::appendEvents(const QJsonArray& items)
{
    for (const auto& v : items) {
        const auto o = v.toObject();
        const QString event = o.value("event").toString();
        const QString taskId = o.value("task_id").toString();
        const qint64 ts = o.value("ts_ms").toVariant().toLongLong();
        const QString line = QString("[%1] %2 %3").arg(ts).arg(event, taskId);
        timelineList_->addItem(line);
        if (ts > lastEventTs_) lastEventTs_ = ts;
        updateNodeStatusFromEvent(o);
        if (event == "dag_node_end") {
            // refresh task runs on end event for more details
            fetchTaskRuns();
        }
    }
}

QColor DagRunMonitorDialog::statusColor(int status) const
{
    switch (status) {
    case 1: return QColor("#1e90ff"); // Running
    case 2: return QColor("#2ecc71"); // Success
    case 3: return QColor("#e74c3c"); // Failed
    case 4: return QColor("#e67e22"); // Skipped
    case 5: return QColor("#e74c3c"); // Canceled
    case 6: return QColor("#e74c3c"); // Timeout
    default: return QColor("#7f8c8d"); // Pending/Unknown
    }
}

QString DagRunMonitorDialog::statusText(int status) const
{
    switch (status) {
    case 0: return tr("Pending");
    case 1: return tr("Running");
    case 2: return tr("Success");
    case 3: return tr("Failed");
    case 4: return tr("Skipped");
    case 5: return tr("Canceled");
    case 6: return tr("Timeout");
    default: return tr("Unknown");
    }
}

void DagRunMonitorDialog::updateNodeStatusFromEvent(const QJsonObject& ev)
{
    const QString event = ev.value("event").toString();
    const QString taskId = ev.value("task_id").toString();
    const auto extra = ev.value("extra").toObject();
    if (event == "dag_node_start") {
        bench_->setNodeStatus(taskId, statusColor(1));
        bench_->setNodeStatusLabel(taskId, statusText(1));
    } else if (event == "dag_node_skipped") {
        bench_->setNodeStatus(taskId, statusColor(4));
        bench_->setNodeStatusLabel(taskId, statusText(4));
    } else if (event == "dag_node_end") {
        const int st = extra.value("status").toInt(2);
        bench_->setNodeStatus(taskId, statusColor(st));
        bench_->setNodeStatusLabel(taskId, statusText(st));
    }
}

void DagRunMonitorDialog::onTimelineTick()
{
    fetchEvents();
    if (++pollCounter_ % 5 == 0) {
        fetchTaskRuns();
    }
}
