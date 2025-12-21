#include "canvasbench.h"
#include <QToolBar>
#include <QAction>
#include <QVBoxLayout>
#include <QDebug>

// 引入你之前写好的头文件
#include "canvasscene.h"
#include "canvasview.h"
#include "commands/undostack.h"
#include "tasks/task_manager.h"

// 引入具体的 Task
#include "tasks/select_task.h"      // 下面会给你
#include "tasks/create_task.h"      // 之前给过，稍作调整
#include "tasks/connect_task.h"
#include "commands/command.h"
#include "tasks/main_task.h"
#include "tasks/delete_task.h"
#include "Item/node_type.h"

CanvasBench::CanvasBench(QWidget* parent)
    : QWidget(parent)
{
    buildCore();
    buildUi();
    wireUi();
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

    // 2. 再压入【默认选择任务】(Level 10)
    // 此时栈结构: [Bottom: MainTask] -> [Top: SelectTask]
    // 当你 push CreateTask(Level 10) 时：
    //   - SelectTask(10) <= CreateTask(10) -> SelectTask 被弹出 (你的新规则)
    //   - MainTask(10000) <= CreateTask(10) -> False -> MainTask 保留
    // 栈变成: [Bottom: MainTask] -> [Top: CreateTask]
    startSelectMode();
    

}

void CanvasBench::buildUi()
{
    // 创建工具栏
    toolbar_ = new QToolBar(this);
    actCreateRect_ = toolbar_->addAction("画矩形");
    actCreateShell_ = toolbar_->addAction("Shell 节点");
    actCreateHttp_ = toolbar_->addAction("HTTP 节点");
    actCreateRemote_ = toolbar_->addAction("Remote 节点");
    actCreateLocal_ = toolbar_->addAction("Local 节点");
    toolbar_->addSeparator();
    actUndo_ = toolbar_->addAction("撤销");
    actRedo_ = toolbar_->addAction("重做");

    actConnect_ = toolbar_->addAction("连线");
    actDelete_  = toolbar_->addAction("删除");

    actUndo_->setShortcut(QKeySequence::Undo);
    actRedo_->setShortcut(QKeySequence::Redo);

    // 布局
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0); // 无边距
    layout->setSpacing(0);
    layout->addWidget(toolbar_);
    layout->addWidget(view_);
}

void CanvasBench::wireUi()
{
    // 按钮 -> 功能
    connect(actCreateRect_, &QAction::triggered, this, &CanvasBench::startCreateRectMode);
    connect(actCreateShell_, &QAction::triggered, this, &CanvasBench::startCreateShellMode);
    connect(actCreateHttp_, &QAction::triggered, this, &CanvasBench::startCreateHttpMode);
    connect(actCreateRemote_, &QAction::triggered, this, &CanvasBench::startCreateRemoteMode);
    connect(actCreateLocal_, &QAction::triggered, this, &CanvasBench::startCreateLocalMode);
    connect(actUndo_, &QAction::triggered, this, &CanvasBench::undo);
    connect(actRedo_, &QAction::triggered, this, &CanvasBench::redo);

    // UndoStack 状态 -> 按钮状态 (Qt 的 QUndoStack 也有 canUndoChanged 信号)
    // 这里假设你的 UndoStack 封装暴露了 internalStack 或者自己发信号
    if (auto* stack = undoStack_->internalStack()) {
        connect(stack, &QUndoStack::canUndoChanged, actUndo_, &QAction::setEnabled);
        connect(stack, &QUndoStack::canRedoChanged, actRedo_, &QAction::setEnabled);
        // 初始化状态
        actUndo_->setEnabled(stack->canUndo());
        actRedo_->setEnabled(stack->canRedo());
    }

    connect(actConnect_, &QAction::triggered, this, &CanvasBench::startConnectMode);
    connect(actDelete_, &QAction::triggered, this, &CanvasBench::deleteSelected);
}

void CanvasBench::startSelectMode()
{
    // 封装一个复用函数，或者直接像 MainTask 里的逻辑那样写
    auto* t = new SelectTask(this);
    t->setView(view_);
    taskMgr_->push(t);
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
void CanvasBench::undo() { undoStack_->undo(); }
void CanvasBench::redo() { undoStack_->redo(); }
