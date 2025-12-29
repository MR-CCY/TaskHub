#include "main_window.h"

#include <QDockWidget>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <QMessageBox>
#include <QStackedWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QAbstractItemView>
#include <QDateTime>
#include <QFont>
#include <QJsonArray>

#include "app_context.h"
#include "net/api_client.h"
#include "net/ws_client.h"
#include "ui/console_dock.h"
#include "ui/user_bar_widget.h"
#include "view/dag_edit_bench.h"
#include "view/dag_runs_widget.h"

MainWindow::MainWindow(QWidget *parent):
    QMainWindow(parent)
{
    setWindowTitle("TaskHub Client");
    resize(1600, 900);
    setWindowState(windowState() | Qt::WindowMaximized);

    setupUserBar();
    setupCentralPages();
    setupNavDock();
    setupConsole();
    setupClients();
    if (workflowPage_) {
        workflowPage_->setApiClient(api_);
    }
    if (runsPage_) {
        runsPage_->setApiClient(api_);
    }
    wireSignals();
}   
     
void MainWindow::onUnauthorized()
{
    QMessageBox::warning(this, tr("登录失效"), tr("登录已过期，请重新登录。"));
    AppContext::instance().setToken("");
    close();   // 关闭主窗口
    emit logoutRequested(); // 通知应用程序显示登录对话框
}

void MainWindow::setupUserBar() {
    userBar_ = new UserBarWidget(this);
    userBar_->setUsername(tr("-"));
    userBar_->setLoginTime(QDateTime::currentDateTime());
    setMenuWidget(userBar_);

    connect(userBar_, &UserBarWidget::logoutClicked, this, [this]() {
        emit logoutRequested();
        close();
    });
    connect(userBar_, &UserBarWidget::switchUserClicked, this, [this]() {
        emit logoutRequested();
    });
}

void MainWindow::setupCentralPages() {
    centralStack_ = new QStackedWidget(this);
    workflowPage_ = new DagEditBench(this);
    templatesPage_ = createPlaceholderPage(tr("Templates Page (TODO)"));
    runsPage_ = new DagRunsWidget(this);
    cronPage_ = createPlaceholderPage(tr("Cron Page (TODO)"));

    centralStack_->addWidget(workflowPage_);
    centralStack_->addWidget(templatesPage_);
    centralStack_->addWidget(runsPage_);
    centralStack_->addWidget(cronPage_);

    setCentralWidget(centralStack_);
}

void MainWindow::setupNavDock() {
    navDock_ = new QDockWidget(tr("导航"), this);
    navDock_->setAllowedAreas(Qt::LeftDockWidgetArea);
    navDock_->setFeatures(QDockWidget::NoDockWidgetFeatures);

    navList_ = new QListWidget(navDock_);
    navList_->addItem(tr("DAG编辑"));
    navList_->addItem(tr("模版编辑"));
    navList_->addItem(tr("DAG管理"));
    navList_->addItem(tr("定时任务"));
    navList_->setSelectionMode(QAbstractItemView::SingleSelection);
    navList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QFont navFont = navList_->font();
    navFont.setPointSize(navFont.pointSize() + 2);
    navFont.setBold(true);
    navList_->setFont(navFont);
    navList_->setSpacing(4);
    // navList_->setMinimumWidth(140);
    navList_->setFixedWidth(100);
    navDock_->setWidget(navList_);

    addDockWidget(Qt::LeftDockWidgetArea, navDock_);
    connect(navList_, &QListWidget::currentRowChanged, this, &MainWindow::onNavIndexChanged);
    navList_->setCurrentRow(0);
}

void MainWindow::setupConsole() {
    console_ = new ConsoleDock(this);
    console_->setAllowedAreas(Qt::BottomDockWidgetArea);
    console_->setFeatures(QDockWidget::NoDockWidgetFeatures);
    addDockWidget(Qt::BottomDockWidgetArea, console_);
    console_->appendInfo("App started");
}

void MainWindow::setupClients() {
    api_ = new ApiClient(this);
    ws_  =  WsClient::instance();
    ws_->setToken( AppContext::instance().token());
    api_->setToken( AppContext::instance().token());
    // 你自己的 baseUrl 放这里（或从 Settings 读）
    api_->setBaseUrl("http://127.0.0.1:8082");
    wsUrl_ = QUrl("ws://127.0.0.1:8090/ws"); 
    ws_->connectTo(wsUrl_)
;

    api_->getHealth();
    api_->getInfo();
   
}
void MainWindow::wireSignals() {
    connect(api_, &ApiClient::unauthorized, this, &MainWindow::onUnauthorized);
    connect(api_, &ApiClient::rawJson, this, [this](const QString& name, const QJsonObject& obj) {
        console_->appendInfo(QString("HTTP %1: %2")
                             .arg(name, QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact))));
    });

    connect(api_, &ApiClient::requestFailed, this, [this](const QString& apiName, int st, const QString& msg) {
        console_->appendError(QString("HTTP %1 failed (%2): %3").arg(apiName).arg(st).arg(msg));
    });

    connect(api_, &ApiClient::healthOk, this, [this](const QJsonObject& data) {
        console_->appendInfo(QString("health ok: %1")
                             .arg(QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact))));
    });

    connect(api_, &ApiClient::infoOk, this, [this](const QJsonObject& data) {
        console_->appendInfo(QString("info ok: %1")
                             .arg(QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact))));
    });

    connect(api_, &ApiClient::loginOk, this, [this](const QString& token, const QString& username) {
        onLoginOk(token, username);
    });

    connect(api_, &ApiClient::loginFailed, this, [this](int st, const QString& msg) {
        console_->appendError(QString("login failed (%1): %2").arg(st).arg(msg));
    });

    connect(ws_, &WsClient::connected, this, [this]() {
        console_->appendInfo("WS connected, sent token");
    });

    connect(ws_, &WsClient::authed, this, [this]() {
        console_->appendInfo("WS authed");
        ws_->subscribeTaskLogs("_system");   // ✅ 先订阅系统日志
    });

    connect(ws_, &WsClient::messageReceived, this, [this](const QJsonObject& obj) {
        const QString type = obj.value("type").toString();

        if (type == "log") {
            const QString taskId = obj.value("task_id").toString();
            const QString msg    = obj.value("message").toString();
            const int level      = obj.value("level").toInt(-1);
            const qint64 tsMs    = obj.value("ts_ms").toVariant().toLongLong();
            console_->appendTaskLog(QString("level=%1 task=%2 ts=%3 %4")
                .arg(level).arg(taskId).arg(tsMs).arg(msg));
            return;
        }

        console_->appendEvent(QString("WS msg: %1")
            .arg(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact))));
    });

    connect(api_, &ApiClient::runDagAsyncOk, this, [this](const QString& runId, const QJsonArray& taskIds) {
        console_->appendInfo(QString("run_async started run_id=%1").arg(runId));
        for (const auto& item : taskIds) {
            if (!item.isObject()) continue;
            const QJsonObject obj = item.toObject();
            const QString tid = obj.value("task_id").toString();
            if (tid.isEmpty()) continue;
            ws_->subscribeTaskLogs(tid, runId);
            ws_->subscribeTaskEvents(tid, runId);
            console_->appendInfo(QString("subscribed logs/events for task %1 run %2").arg(tid, runId));
        }
    });

    connect(ws_, &WsClient::error, this, [this](const QString& msg) {
        console_->appendError(QString("WS error: %1").arg(msg));
    });

    connect(ws_, &WsClient::closed, this, [this]() {
        console_->appendError("WS closed");
    });
}

void MainWindow::onLoginOk(const QString& token, const QString& username) {
    console_->appendInfo(QString("login ok: %1").arg(username));

    api_->setToken(token);
    ws_->setToken(token);

    api_->getHealth();
    api_->getInfo();

    if (userBar_) {
        userBar_->setUsername(username);
        userBar_->setLoginTime(QDateTime::currentDateTime());
    }

    // connect WS after login
    ws_->connectTo(wsUrl_);
}

void MainWindow::onNavIndexChanged(int index) {
    if (!centralStack_) return;
    if (index >= 0 && index < centralStack_->count()) {
        centralStack_->setCurrentIndex(index);
    }
}

QWidget* MainWindow::createPlaceholderPage(const QString& text) {
    auto* widget = new QWidget(this);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addStretch();
    layout->addWidget(new QLabel(text, widget), 0, Qt::AlignHCenter);
    layout->addStretch();
    return widget;
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    return false;
}
