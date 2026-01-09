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
#include "ui/console_dock.h"
#include "net/ws_client.h"
#include <QDebug>
/**
 * @brief 解析JSON字节数组为QJsonObject
 * 
 * @param bytes 包含JSON数据的字节数组
 * @return QJsonObject 解析后的JSON对象，如果解析失败则返回空对象
 */
namespace {
QJsonObject parseJson(const QByteArray& bytes) {
    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(bytes, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return {};
    return doc.object();
}
}

/**
 * @brief 构造函数，初始化DAG运行监控对话框
 * 
 * @param runId DAG运行的唯一标识符
 * @param dagJson DAG的JSON描述字符串
 * @param api API客户端指针，用于网络请求
 * @param parent 父窗口部件
 */
DagRunMonitorDialog::DagRunMonitorDialog(const QString& runId,const QString& dagJson, ApiClient* api,QWidget* parent)
    : QDialog(parent),runId_(runId),dagJson_(dagJson),api_(api)
{
    setModal(true);
    setWindowTitle(tr("DAG 监控 - %1").arg(runId_));
    resize(1200, 800);
    buildUi();

    // 设置定时器，用于定期获取时间线更新
    pollTimer_.setInterval(500);
    connect(&pollTimer_, &QTimer::timeout, this, &DagRunMonitorDialog::onTimelineTick);
    pollTimer_.start();

    // 初始获取任务运行信息和事件
    fetchTaskRuns();
    fetchEvents();
    
    // 订阅任务日志和事件的WebSocket消息
    WsClient::instance()->subscribeTaskLogs("", runId);
    WsClient::instance()->subscribeTaskEvents("", runId);
    connect(WsClient::instance(), &WsClient::messageReceived, this, [this](const QJsonObject& obj){
        const QString type = obj.value("type").toString();
        const QString topic = obj.value("topic").toString();
        const QString taskId = obj.value("task_id").toString();
        const QString runId = obj.value("run_id").toString();
        if (!runId.isEmpty() && runId != runId_) return; // 只关心当前 run
        if (type == "log" || topic == "task_logs") {
            consoleDock_->appendTaskLog(obj.value("message").toString());
        } else if (type == "event" || topic == "task_events") {
            consoleDock_->appendEvent(QJsonDocument(obj).toJson(QJsonDocument::Compact));
        } else {
            consoleDock_->appendInfo(QJsonDocument(obj).toJson(QJsonDocument::Compact));
        }
    });

    // Connect ApiClient signals
    if (api_) {
        // connect(api_, &ApiClient::taskRunsOk, this, &DagRunMonitorDialog::onTaskRuns);
        // Using unique connection to avoid duplicates if re-binding
        connect(api_, &ApiClient::taskRunsOk, this, &DagRunMonitorDialog::onTaskRuns, Qt::UniqueConnection);
        connect(api_, &ApiClient::remoteTaskRunsOk, this, &DagRunMonitorDialog::onRemoteTaskRuns, Qt::UniqueConnection);
        connect(api_, &ApiClient::dagEventsOk, this, &DagRunMonitorDialog::onDagEvents, Qt::UniqueConnection);
        connect(api_, &ApiClient::remoteDagEventsOk, this, &DagRunMonitorDialog::onRemoteDagEvents, Qt::UniqueConnection);
    }
}

/**
 * @brief 析构函数，清理WebSocket订阅
 */
DagRunMonitorDialog::~DagRunMonitorDialog(){
    WsClient::instance()->unsubscribeTaskLogs("", runId_);
    WsClient::instance()->unsubscribeTaskEvents("", runId_);
};

/**
 * @brief 构建用户界面
 * 
 * 创建主布局、分割窗口、DAG运行查看器、检查器面板和控制台停靠窗口
 */
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

    consoleDock_ = new ConsoleDock(this);
    consoleDock_->setFloating(false);
    consoleDock_->setFeatures(QDockWidget::NoDockWidgetFeatures);
    consoleDock_->setTitleBarWidget(new QWidget(consoleDock_));
    consoleDock_->hideConsoleTab();
    mainLay->addWidget(consoleDock_->widget(), 2);
}

/**
 * @brief 从API获取任务运行信息
 * 
 * 发起网络请求获取与当前运行ID相关的任务运行信息
 */
void DagRunMonitorDialog::fetchTaskRuns()
{
    if (!api_) return;
    // 1. Fetch Local
    api_->getTaskRuns(runId_, "", 500);

    // 2. Fetch Remote
    if (bench_) {
        QStringList remotes = bench_->getRemoteNodes();
        for (const auto& rPath : remotes) {
            fetchRemoteTaskRuns(rPath);
        }
    }
}

void DagRunMonitorDialog::fetchRemoteTaskRuns(const QString& remotePath)
{
    if (!api_) return;
    api_->getRemoteTaskRuns(runId_, remotePath, 500);
}

void DagRunMonitorDialog::onTaskRuns(const QJsonArray& items)
{
    // Filter by runId if needed? Usually for current run.
    // For now we assume items overlap is small risk or acceptable.
    // Ideally api request context is needed but we rely on runId embedded in data if strict.
    // But items are bare JSON. 
    // Simply merge as local.
    mergeRemoteTaskRuns("", items);
}

void DagRunMonitorDialog::onRemoteTaskRuns(const QString& remotePath, const QJsonArray& items)
{
    mergeRemoteTaskRuns(remotePath, items);
}

/**
 * @brief 使用获取到的任务运行信息填充界面
 * 
 * @param items 任务运行信息的JSON数组
 */
void DagRunMonitorDialog::mergeRemoteTaskRuns(const QString& remotePrefix, const QJsonArray& items)
{
    for (const auto& v : items) {
        auto o = v.toObject();
        QString originalId = o.value("logical_id").toString();
        
        // Prefix ID
        QString fullId = originalId;
        if (!remotePrefix.isEmpty()) {
            fullId = remotePrefix + "/" + originalId;
        }
        o["logical_id"] = fullId; // update in object

        // Update cache
        allTaskRuns_[fullId] = o;

        // Update bench status immediately
        const int status = o.value("status").toInt();
        bench_->setNodeStatus(fullId, statusColor(status));
        bench_->setNodeStatusLabel(fullId, statusText(status));
    }

    // Refresh inspector list from ALL cached runs
    QJsonArray mergedItems;
    for (auto it = allTaskRuns_.begin(); it != allTaskRuns_.end(); ++it) {
        mergedItems.append(it.value());
    }
    if (!mergedItems.isEmpty()) {
        inspector_->setTaskRuns(mergedItems);
    }
}

void DagRunMonitorDialog::populateTaskRuns(const QJsonArray& items)
{
    // Deprecated, redirected to merge
    mergeRemoteTaskRuns("", items);
}

/**
 * @brief 从API获取事件信息
 * 
 * 发起网络请求获取与当前运行ID相关的事件，支持时间戳过滤
 */
void DagRunMonitorDialog::fetchEvents()
{
    if (!api_) return;
    // 1. Local Events
    api_->getDagEvents(runId_, "", "", "", lastEventTs_ > 0 ? (lastEventTs_ + 1) : 0, 0, 200);

    // 2. Remote Events
    if (bench_) {
        QStringList remotes = bench_->getRemoteNodes();
        for (const auto& rPath : remotes) {
            fetchRemoteEvents(rPath);
        }
    }
}

void DagRunMonitorDialog::fetchRemoteEvents(const QString& remotePath) {
    if (!api_) return;
    qint64 lastTs = remoteLastEventTs_.value(remotePath, 0);
    api_->getRemoteDagEvents(runId_, remotePath, lastTs > 0 ? (lastTs + 1) : 0, 200);
}

void DagRunMonitorDialog::onDagEvents(const QJsonArray& items)
{
    mergeRemoteEvents("", items);
}

void DagRunMonitorDialog::onRemoteDagEvents(const QString& remotePath, const QJsonArray& items)
{
    mergeRemoteEvents(remotePath, items);
}

/**
 * @brief 将获取到的事件添加到控制台
 * 
 * @param items 事件信息的JSON数组
 */
void DagRunMonitorDialog::mergeRemoteEvents(const QString& remotePrefix, const QJsonArray& items)
{
     for (const auto& v : items) {
        auto o = v.toObject();
        QString originalId = o.value("task_id").toString();
        // Prefix ID
        QString fullId = originalId;
         if (!remotePrefix.isEmpty()) {
            fullId = remotePrefix + "/" + originalId;
        }
        o["task_id"] = fullId; // update for status update logic

        const QString event = o.value("event").toString();
        const qint64 ts = o.value("ts_ms").toVariant().toLongLong();
        
        // Console Log
        const QString line = QString("[%1] %2 %3").arg(remotePrefix.isEmpty() ? "L" : remotePrefix, event, fullId);
        consoleDock_->appendTimeline(line);

        // Update Last TS
        if (remotePrefix.isEmpty()) {
             if (ts > lastEventTs_) lastEventTs_ = ts;
        } else {
             if (ts > remoteLastEventTs_.value(remotePrefix, 0)) {
                 remoteLastEventTs_[remotePrefix] = ts;
             }
        }
        
        updateNodeStatusFromEvent(o);
        if (event == "dag_node_end") {
            // refresh runs? handled by poll timer anyway
        }
    }
}

void DagRunMonitorDialog::appendEvents(const QJsonArray& items)
{
    mergeRemoteEvents("", items);
}

/**
 * @brief 根据状态码获取对应的颜色
 * 
 * @param status 任务状态码
 * @return QColor 表示状态的颜色
 */
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

/**
 * @brief 根据状态码获取对应的状态文本
 * 
 * @param status 任务状态码
 * @return QString 表示状态的文本
 */
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

/**
 * @brief 根据事件更新节点状态
 * 
 * @param ev 包含事件信息的JSON对象
 */
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

/**
 * @brief 时间线定时器处理函数
 * 
 * 定期获取事件和任务运行信息以更新界面
 */
void DagRunMonitorDialog::onTimelineTick()
{
    fetchEvents();
    if (++pollCounter_ % 5 == 0) {
        fetchTaskRuns();
    }
}