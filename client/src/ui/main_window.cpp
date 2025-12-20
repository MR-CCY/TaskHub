#include "main_window.h"
#include <QPushButton>
#include <QJsonArray>
#include <QJsonObject>
#include <QHeaderView>
#include <QToolBar>
#include <QMouseEvent>
#include "new_task_dialog.h"
#include "taskdetaildialog.h"
#include <QMessageBox>
#include "app_context.h"
#include "core/task_ws_client.h"
#include "view/workflow_bench.h"

MainWindow::MainWindow(QWidget *parent):
    QMainWindow(parent)
{
    setWindowTitle("TaskHub Client");
    resize(2000,2000);
    bench_ = new WorkflowBench(this);
    setCentralWidget(bench_);
    setupConsole();
    setupClients();
    wireSignals();
}   
     
void MainWindow::onUnauthorized()
{
    QMessageBox::warning(this, "登录失效", "登录已过期，请重新登录。");
    AppContext::instance().setToken("");
    close();   // 关闭主窗口
    emit logoutRequested(); // 通知应用程序显示登录对话框
}


void MainWindow::setupConsole() {
    console_ = new ConsoleDock(this);
    addDockWidget(Qt::BottomDockWidgetArea, console_);
    console_->appendInfo("App started");
}

void MainWindow::setupClients() {
    api_ = new ApiClient(this);
    ws_  = new WsClient(this);
    ws_->setToken( AppContext::instance().token());
    api_->setToken( AppContext::instance().token());

    // 你自己的 baseUrl 放这里（或从 Settings 读）
    api_->setBaseUrl("http://127.0.0.1:8082");
    wsUrl_ = QUrl("ws://127.0.0.1:8090/ws"); 
    ws_->connectTo(wsUrl_);

    api_->getHealth();
    api_->getInfo();
   
}
void MainWindow::wireSignals() {
    // ApiClient raw json -> console

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

    // WsClient -> console
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
        console_->appendInfo(QString("LOG[%1] task=%2 ts=%3 %4")
            .arg(level).arg(taskId).arg(tsMs).arg(msg));
        return;
    }

    // 非 log 才输出 raw（事件/调试/未知消息）
    console_->appendInfo(QString("WS msg: %1")
        .arg(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact))));
    });

    connect(ws_, &WsClient::error, this, [this](const QString& msg) {
        console_->appendError(QString("WS error: %1").arg(msg));
    });

    connect(ws_, &WsClient::closed, this, [this]() {
        console_->appendError("WS closed");
    });

    connect(bench_, &WorkflowBench::debugUndoStateChanged, this,
        [this](bool u, bool r, const QString& ut, const QString& rt){
        console_->appendInfo(QString("[UNDO] canUndo=%1 canRedo=%2 undoTop=%3 redoTop=%4")
        .arg(u ? "true" : "false")
        .arg(r ? "true" : "false")
        .arg(ut.isEmpty() ? "-" : ut)
        .arg(rt.isEmpty() ? "-" : rt));
});
}

void MainWindow::onLoginOk(const QString& token, const QString& username) {
    console_->appendInfo(QString("login ok: %1").arg(username));

    api_->setToken(token);
    ws_->setToken(token);

    api_->getHealth();
    api_->getInfo();

    // connect WS after login
    ws_->connectTo(wsUrl_);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    return false;
}
