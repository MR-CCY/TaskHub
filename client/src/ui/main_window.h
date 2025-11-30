#pragma once
#include <QMainWindow>
#include <QTableView>
#include <QEvent>
#include "http_client.h"
#include "task_list_model.h"
#include "ui_main_windows.h"
#include "task_list_model.h"

class TaskWsClient;
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
signals:
    void logoutRequested(); // 当需要重新登录时发出
private slots:
    void onRefreshTasks();
    void onNewTask();
    void onUnauthorized();
protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
private:
    HttpClient       m_http;
    TaskListModel*   m_model  = nullptr;
    QTableView*      m_view  = nullptr;
    TaskWsClient*    m_wsClient  = nullptr;
    Ui::MainWindowUi*  ui=nullptr;
    
    void setupWs();
    
};
