#include "main_window.h"
#include <QPushButton>
#include <QJsonArray>
#include <QJsonObject>
#include "new_task_dialog.h"
MainWindow::MainWindow(QWidget *parent):
    QMainWindow(parent),
    m_http(this)
{
    setWindowTitle("TaskHub Client");
    resize(800,600);
    m_view= new QTableView(this);
    ui=new Ui::MainWindowUi();
    ui->setupUi(this);
    m_view=new QTableView(this);
    ui->widget->layout()->addWidget(m_view); 
    m_model=new TaskListModel(this);
    m_view->setModel(m_model);
    connect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::onRefreshTasks);
    connect(ui->pushButton_2, &QPushButton::clicked, this, &MainWindow::onNewTask);
    onRefreshTasks();
}


void MainWindow::onNewTask()
{
    NewTaskDialog dlg(this);
    if(dlg.exec()!=QDialog::Accepted){
        return;
    }
    QJsonObject body;
    body["name"]=dlg.taskName();
    body["cmd"]=dlg.cmd();

    m_http.postJson("/api/tasks", body, [this](bool ok, QJsonObject resp){
        if(!ok||resp["code"] != 0){
            ui->statusbar->showMessage("创建任务失败："+resp["message"].toString(), 5000);
            return;
        }
        ui->statusbar->showMessage("创建任务成功", 5000);
        onRefreshTasks();
    });
}
void MainWindow::onRefreshTasks(){
    m_http.getJson("/api/tasks", [this](bool ok, QJsonObject resp){
        if(!ok||resp["code"] != 0){
            ui->statusbar->showMessage("获取任务列表失败："+resp["message"].toString(), 5000);
            return;
        }
        QJsonArray tasks=resp["data"].toArray();
        QList<TaskItem> items;
        for(const QJsonValue & val: tasks){
            QJsonObject obj=val.toObject();
            TaskItem item;
            item.id=obj["id"].toInt();
            item.name=obj["name"].toString();
            item.status=obj["status"].toString();
            item.createTime=obj["create_time"].toString();
            item.updateTime=obj["update_time"].toString();
            // item.params=obj["params"].toString();
            items.append(item);
        }
        m_model->setTasks(items);
        ui->statusbar->showMessage(QString("获取到 %1 个任务").arg(items.size()), 5000);
    });



}
