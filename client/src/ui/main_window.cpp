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

MainWindow::MainWindow(QWidget *parent):
    QMainWindow(parent),
    m_http(this)
{
    setWindowTitle("TaskHub Client");
    resize(800,600);
    m_view= new QTableView(this);
    ui=new Ui::MainWindowUi();
    ui->setupUi(this);
    ui->centralwidget->installEventFilter(this);
    ui->widget->installEventFilter(this);
    ui->centralwidget->setMouseTracking(true);
    ui->widget->setMouseTracking(true);
    ui->centralwidget->setAutoFillBackground(true);
    
    QPalette pal = ui->centralwidget->palette();
    pal.setColor(QPalette::Window, Qt::white);
    ui->centralwidget->setPalette(pal);
    m_view=new QTableView(this);
    ui->widget->layout()->addWidget(m_view); 
    m_model=new TaskListModel(this);
    m_view->setModel(m_model);
    m_view->verticalHeader()->setVisible(false); // hide row numbers column
    m_view->setCornerButtonEnabled(false);       // hide top-left square corner

    connect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::onRefreshTasks);
    connect(ui->pushButton_2, &QPushButton::clicked, this, &MainWindow::onNewTask);
    connect(m_view, &QTableView::doubleClicked, [this](const QModelIndex & index){
        if(!index.isValid()){
            return;
        }
        TaskDetailDialog dlg(this);
        dlg.setTaskDetail( m_model->taskAt(index.row()) );
        dlg.exec();
    });
    connect(&m_http, &HttpClient::unauthorized, this, &MainWindow::onUnauthorized);
    onRefreshTasks();
    QString info = QString("用户：%1   服务器：%2   登陆时间：%3")
    .arg(AppContext::instance().username())
    .arg(AppContext::instance().baseUrl())
    .arg(AppContext::instance().loginTime());

    auto label = new QLabel(info, this);
    statusBar()->addPermanentWidget(label);

    for (auto child : centralWidget()->children()) {
        qDebug() << "Child:" << child << child->metaObject()->className();
    }
    m_wsClient = new TaskWsClient(this);
    setupWs();
}


void MainWindow::onNewTask()
{
    NewTaskDialog dlg(this);
    if(dlg.exec()!=QDialog::Accepted){
        return;
    }
    QJsonObject body;
    body["name"]=dlg.taskName();
    body["params"]= QJsonObject{{"cmd", dlg.cmd()}};

    m_http.postJson("/api/tasks", body, [this](bool ok, QJsonObject resp){
        if(!ok||resp["code"] != 0){
            ui->statusbar->showMessage("创建任务失败："+resp["message"].toString(), 5000);
            return;
        }
        ui->statusbar->showMessage("创建任务成功", 5000);
        onRefreshTasks();
    });
}
void MainWindow::onUnauthorized()
{
    QMessageBox::warning(this, "登录失效", "登录已过期，请重新登录。");
    AppContext::instance().setToken("");
    close();   // 关闭主窗口
    emit logoutRequested(); // 通知应用程序显示登录对话框
}
void MainWindow::onRefreshTasks()
{
    ui->pushButton->setEnabled(false);
    statusBar()->showMessage("正在加载任务列表...");

        
    m_http.getJson("/api/tasks", [this](bool ok, QJsonObject resp){
        ui->pushButton->setEnabled(true);
        statusBar()->clearMessage();
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
            item.cmd=obj["params"].toObject()["cmd"].toString();
            item.createTime=obj["create_time"].toString();
            item.updateTime=obj["update_time"].toString();
            item.exitCode=obj["exit_code"].toInt();
            item.output=obj["last_output"].toString();
            item.errorMsg=obj["last_error"].toString();
            items.append(item);
        }
        m_model->setTasks(items);
        ui->statusbar->showMessage(QString("获取到 %1 个任务").arg(items.size()), 5000);
    });
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        QWidget* target = nullptr;

        if (obj == ui->centralwidget) {
            target = ui->centralwidget->childAt(me->pos());
        } else if (obj == ui->widget) {
            target = ui->widget->childAt(me->pos());
        } else if (auto* w = qobject_cast<QWidget*>(obj)) {
            target = w->childAt(w->mapFromGlobal(me->globalPos()));
        }

        QString cls = target ? target->metaObject()->className() : QStringLiteral("<none>");
        QString name = target ? target->objectName() : QStringLiteral("<none>");
        statusBar()->showMessage(QString("Clicked widget: %1 (objectName=%2)").arg(cls, name), 5000);
    }

    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::setupWs()
{
    connect(m_wsClient, &TaskWsClient::taskCreated, m_model, &TaskListModel::upsertFromJson);
    connect(m_wsClient, &TaskWsClient::taskUpdated, m_model, &TaskListModel::upsertFromJson);
    connect(m_wsClient, &TaskWsClient::connected, this, []{
        qDebug() << "[UI] WS connected";
    });
    connect(m_wsClient, &TaskWsClient::disconnected, this, []{
        qDebug() << "[UI] WS disconnected";
    });
    connect(m_wsClient, &TaskWsClient::errorOccurred, this, [](const QString &msg){
        qWarning() << "[UI] WS error:" << msg;
    });

// 暂时先在启动时直接连本机，后面你可以改成根据配置 / 登录信息拼 URL
m_wsClient->connectToServer(QUrl(QStringLiteral("ws://127.0.0.1:8090/ws/tasks")));
}
