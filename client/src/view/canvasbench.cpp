#include "canvasbench.h"
#include <QDebug>
#include <QFileDialog>
#include <QFile>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QHash>
#include <QList>
#include <QVariant>

// 引入你之前写好的头文件
#include "canvasscene.h"
#include "canvasview.h"
#include "commands/undostack.h"
#include "tasks/task_manager.h"

// 引入具体的 Task
#include "tasks/select_task.h"    
#include "tasks/create_task.h" 
#include "tasks/connect_task.h"
#include "commands/command.h"
#include "tasks/main_task.h"
#include "tasks/delete_task.h"
#include "tasks/import_task.h"
#include "tasks/layout_task.h"
#include "tasks/zoom_task.h"
#include "tasks/edit_node_task.h"
#include "Item/node_type.h"
#include "Item/rect_item.h"
#include "Item/line_item.h"
#include "Item/line_item_factory.h"
#include "dag_serializer.h"

CanvasBench::CanvasBench(QWidget* parent)
    : QWidget(parent)
{
    buildCore();
}

CanvasBench::~CanvasBench() = default;

void CanvasBench::buildCore()
{
    // 1. 创建基础设施
    undoStack_ = new UndoStack(this);
    scene_ = new CanvasScene(this);
    taskMgr_ = new TaskManager(this);
    
    // 2. 创建视图并注入依赖
    view_ = new CanvasView(this);
    view_->setScene(scene_);
    view_->setUndoStack(undoStack_);
    view_->setTaskManager(taskMgr_); // 关键：View 拿到 TaskMgr 直接派发事件

    // 3. 启动默认任务 (SelectTask)
    // SelectTask 是 Level 0，常驻栈底
    // ... (基础设施初始化保持不变) ...

    // 1. 先压入【兜底任务】(Level 10000)
    // 它是栈的基座，永远存在
    auto* mainTask = new MainTask(this);
    mainTask->setView(view_);
    taskMgr_->push(mainTask);
    startZoomTask();

    // 2. 再压入【默认选择任务】(Level 10)
    // 此时栈结构: [Bottom: MainTask] -> [Top: SelectTask]
    // 当你 push CreateTask(Level 10) 时：
    //   - SelectTask(10) <= CreateTask(10) -> SelectTask 被弹出 (你的新规则)
    //   - MainTask(10000) <= CreateTask(10) -> False -> MainTask 保留
    // 栈变成: [Bottom: MainTask] -> [Top: CreateTask]
    startSelectMode();
    

}
void CanvasBench::startSelectMode()
{
    // 封装一个复用函数，或者直接像 MainTask 里的逻辑那样写
    auto* t = new SelectTask(this);
    t->setView(view_);
    taskMgr_->push(t);
}

void CanvasBench::startZoomTask() {
    auto* z = new ZoomTask(this);
    z->setView(view_);
    taskMgr_->push(z);
}

void CanvasBench::startCreateRectMode()
{
    // 创建一个新的临时任务
    // Level 100 (比 SelectTask 高)，压栈时会接管输入
    auto* task = new CreateTask(this);
    task->setView(view_);
    
    qDebug() << "Pushing CreateTask...";
    taskMgr_->push(task); 
}
void CanvasBench::startCreateNodeMode(NodeType type) {
    auto* task = new CreateTask(type, this);
    task->setView(view_);
    qDebug() << "Pushing CreateTask for node type" << static_cast<int>(type);
    taskMgr_->push(task);
}

void CanvasBench::startCreateShellMode() { startCreateNodeMode(NodeType::Shell); }
void CanvasBench::startCreateHttpMode()  { startCreateNodeMode(NodeType::Http); }
void CanvasBench::startCreateRemoteMode(){ startCreateNodeMode(NodeType::Remote); }
void CanvasBench::startCreateLocalMode() { startCreateNodeMode(NodeType::Local); }
void CanvasBench::startConnectMode() {
    // 开启连线任务，Level 200，压在 Level 10 的 SelectTask 上
    // 这样 SelectTask 就收不到事件了（实现了“连线任务期间不可触发移动任务”）
    auto* task = new ConnectTask(this);
    // task->setView(view_);
    taskMgr_->push(task);
}

void CanvasBench::deleteSelected() {
    auto* task = new DeleteTask(scene_, undoStack_, this);
    taskMgr_->push(task);
    task->execute();
}

void CanvasBench::importJson() {
    auto* task = new ImportTask(scene_, undoStack_, view_, this);
    taskMgr_->push(task);
    task->execute();
}
void CanvasBench::editSelectedNode() {
    auto* task = new EditNodeTask(scene_, undoStack_, view_, this);
    taskMgr_->push(task);
    task->execute();
}

void CanvasBench::layoutDag() {
    auto* task = new LayoutTask(scene_, this);
    taskMgr_->push(task);
    task->execute();
}

void CanvasBench::exportJson() {
    const QJsonObject root = buildDagJson(scene_, "SkipDownstream", 4);
    if (root.isEmpty()) return;

    const QString path = QFileDialog::getSaveFileName(this, tr("导出 JSON"), QString(), tr("JSON Files (*.json);;All Files (*)"));
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "Failed to open file for export" << path;
        return;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
}
void CanvasBench::undo() { undoStack_->undo(); }
void CanvasBench::redo() { undoStack_->redo(); }
