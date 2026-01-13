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
#include <QStringList>

#include "dag_run_viewer.h"
#include "net/api_client.h"
#include "view/inspector/inspector_panel.h"
#include "ui/console_dock.h"
#include "net/ws_client.h"
#include <QDebug>
#include "Item/rect_item.h"
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
    const QJsonDocument doc = QJsonDocument::fromJson(dagJson_.toUtf8());
    if (doc.isObject()&&bench_) {
        bench_->loadDagJson(doc.object());
    }
    // 设置定时器，用于定期获取时间线更新
    pollTimer_.setInterval(500);
    connect(&pollTimer_, &QTimer::timeout, this, &DagRunMonitorDialog::onTimelineTick);
    pollTimer_.start();

    // 初始获取任务运行信息和事件
    fetchTaskRuns();
    
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
        connect(api_, &ApiClient::remoteTaskRunsOk, this, &DagRunMonitorDialog::onRemoteTaskRuns, Qt::UniqueConnection);
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
    splitter->addWidget(bench_);

    inspector_ = new InspectorPanel(bench_->canvasScene(), bench_->benchUndo(), bench_->benchView(), this);
    inspector_->setApiClient(api_);
    inspector_->setReadOnlyMode(true);
    // 不限制最大宽度，让用户可以调整
    splitter->addWidget(inspector_);

    // 设置初始大小：画布900px，inspector 300px
    splitter->setSizes({900, 300});
    splitter->setStretchFactor(0, 3);  // 画布优先扩展
    splitter->setStretchFactor(1, 1);  // inspector次要
    
    mainLay->addWidget(splitter, 10);  // 增加权重，让画布区域占更多垂直空间

    consoleDock_ = new ConsoleDock(this);
    consoleDock_->setFloating(false);
    consoleDock_->setFeatures(QDockWidget::NoDockWidgetFeatures);
    consoleDock_->setTitleBarWidget(new QWidget(consoleDock_));
    consoleDock_->hideConsoleTab();
    mainLay->addWidget(consoleDock_->widget(), 1);  // 减少权重，让控制台占较少垂直空间
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
    api_->getRemoteTaskRuns(runId_, "", 500);
    api_->getRemoteDagEvents(runId_, "", 0, 200);

    // 2. Fetch Remote & Nested DAGs
    if (!bench_) return;
    const auto& idMap = bench_->getIdMap();
    qDebug() << "Fetching task runs for:" << idMap.keys();
    for (auto it = idMap.begin(); it != idMap.end(); ++it) {
        RectItem* node = it.value();
        if (!node) continue;

        QString execType = node->propByKeyPath("exec_type").toString();
        if (execType.compare("Remote", Qt::CaseInsensitive) == 0 || execType.compare("Dag", Qt::CaseInsensitive) == 0) {
            QString dagRunId = node->propByKeyPath("exec_params.run_id").toString();
            if (dagRunId.isEmpty()) continue;
            dagNodeRunIds_[dagRunId] = it.key();

            api_->getRemoteTaskRuns(runId_, it.key(), 500);
            api_->getRemoteDagEvents(runId_, it.key(), 0, 200);
        }
    }
}

void DagRunMonitorDialog::onRemoteTaskRuns(const QString& remotePath, const QJsonArray& items)
{
    mergeRemoteTaskRuns(items);
}

/**
 * @brief 使用获取到的任务运行信息填充界面
 * 
 * @param items 任务运行信息的JSON数组
 */
void DagRunMonitorDialog::mergeRemoteTaskRuns(const QJsonArray& items)
{
    for (const auto& v : items) {
        auto o = v.toObject();

        QString runId= o.value("run_id").toString();
        if(runId.isEmpty()) continue;
        QString fullId = o.value("task_id").toString();
        if(dagNodeRunIds_.contains(runId)){
            fullId = dagNodeRunIds_[runId] + "/" + fullId;
        }
        allTaskRuns_[fullId] = o;

        // Update bench status immediately
        const int status = o.value("status").toInt();
        bench_->setNodeStatus(fullId, statusColor(status));
        bench_->setNodeStatusLabel(fullId, statusText(status));
        if(bench_->getIdMap().contains(fullId)){
            auto node = bench_->getIdMap()[fullId];
            node->setProp("result.status", status);
            node->setProp("result.duration_ms", o.value("duration_ms").toInt());
            node->setProp("result.exit_code", o.value("exit_code").toInt());
            node->setProp("result.attempt", o.value("attempt").toInt());
            node->setProp("result.max_attempts", o.value("max_attempts").toInt());
            node->setProp("result.start_ts_ms", o.value("start_ts_ms").toVariant().toLongLong());
            node->setProp("result.end_ts_ms", o.value("end_ts_ms").toVariant().toLongLong());
            node->setProp("result.message", o.value("message").toString());
            node->setProp("result.stdout", o.value("stdout").toString());
            node->setProp("result.stderr", o.value("stderr").toString());
        } 
    }

    inspector_->refreshCurrent();
}


void DagRunMonitorDialog::onRemoteDagEvents(const QString& remotePath, const QJsonArray& items)
{
    mergeRemoteEvents(items);
}

/**
 * @brief 将获取到的事件添加到控制台
 * 
 * @param items 事件信息的JSON数组
 */
void DagRunMonitorDialog::mergeRemoteEvents(const QJsonArray& items)
{
     for (const auto& v : items) {
        auto o = v.toObject();
        QString runId = o.value("run_id").toString();
        if (runId.isEmpty()) continue;
        QString fullId = o.value("task_id").toString();
        if (dagNodeRunIds_.contains(runId)) {
            fullId = dagNodeRunIds_[runId] + "/" + fullId;
        }
        const QString event = o.value("event").toString();
        const qint64 ts = o.value("ts_ms").toVariant().toLongLong();

        // Console Log
        const QString line = QString("[%1] %2").arg(event, fullId);
        consoleDock_->appendTimeline(line);

        int st = o.value("status").toInt(2);
        if ((!nodeLastEvntTs_.contains(fullId)) || nodeLastEvntTs_[fullId] < ts) {
            nodeLastEvntTs_[fullId] = ts;
        }
    }
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

void DagRunMonitorDialog::updateNodeStatusFromEvent(const QString& event, const QString& fullNodeId, const int& st)
{
    if (event == "dag_node_start") {
        bench_->setNodeStatus(fullNodeId, statusColor(1));
        bench_->setNodeStatusLabel(fullNodeId, statusText(1));
    } else if (event == "dag_node_skipped") {
        bench_->setNodeStatus(fullNodeId, statusColor(4));
        bench_->setNodeStatusLabel(fullNodeId, statusText(4));
    } else if (event == "dag_node_end") {
        bench_->setNodeStatus(fullNodeId, statusColor(st));
        bench_->setNodeStatusLabel(fullNodeId, statusText(st));
    }
}

/**
 * @brief 时间线定时器处理函数
 * 
 * 定期获取事件和任务运行信息以更新界面
 */
void DagRunMonitorDialog::onTimelineTick()
{
    if (++pollCounter_ % 5 == 0) {
        fetchTaskRuns();
    }
}