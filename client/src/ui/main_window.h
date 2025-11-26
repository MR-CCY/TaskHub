#pragma once
#include <QMainWindow>
#include <QTableView>
#include "http_client.h"
#include "task_list_model.h"
#include "ui_main_windows.h"
#include "task_list_model.h"
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onRefreshTasks();
    void onNewTask();

private:
    HttpClient       m_http;
    TaskListModel*   m_model;
    QTableView*      m_view=nullptr;
    Ui::MainWindowUi*  ui=nullptr;
    
};