#pragma once
#include <QMainWindow>
#include <QTableView>
#include <QEvent>
#include "http_client.h"
#include "task_list_model.h"
#include "ui_main_windows.h"
#include "task_list_model.h"
#include "net/api_client.h"
#include "net/ws_client.h"
#include "ui/console_dock.h"
class TaskWsClient;
class WorkflowBench;
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
signals:
    void logoutRequested(); // 当需要重新登录时发出
private slots:
    void onUnauthorized();
    void onLoginOk(const QString& token, const QString& username);
protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
private:
    void setupConsole();
    void setupClients();
    void wireSignals();

    Ui::MainWindowUi*  ui=nullptr;
    WorkflowBench* bench_= nullptr;

    ConsoleDock* console_ = nullptr;
    ApiClient* api_ = nullptr;
    WsClient* ws_ = nullptr;

    QUrl wsUrl_= QUrl("ws://127.0.0.1:8090/ws"); 

};
