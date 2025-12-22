#include "main_task.h"
#include "task_manager.h"
#include "select_task.h" // 必须引用默认任务
#include "view/canvasview.h"  // 需要给 SelectTask 注入 View
#include "zoom_task.h"
#include <QDebug>

MainTask::MainTask(QObject* parent)
    : Task(10000, parent) // 10000 分，绝对的“压舱石”
{
}

bool MainTask::dispatch(QEvent* e)
{
    // 1. 检查堆栈状态
    // 如果我是 Top，说明 SelectTask 刚才被 CreateTask 踢了，
    // 然后 CreateTask 自己又 removeSelf() 走了，现在栈里只剩我了。
    if (manager()->top() == this) {
        qDebug() << "[MainTask] Stack is empty (only root left). Restoring SelectTask...";

        // 2. 复活 SelectTask
        auto* zoom = new ZoomTask(manager());
        zoom->setView(view());
        auto* select = new SelectTask(manager()); // parent 给 manager 自动管理
        select->setView(view()); // 关键：必须注入 View，否则 SelectTask 没法干活
        
        // 3. 压栈
        // 先压 zoom（15），再压 select（10）不会把 zoom 弹掉
        manager()->push(zoom);
        manager()->push(select);

        // 4. 事件透传
        // 刚才那个触发检测的鼠标/键盘事件，不能吞掉，要传给新复活的 SelectTask 处理
        return manager()->top()->dispatch(e);
    }

    // 5. (可选) 全局快捷键处理
    // 如果上面的任务都没处理事件（比如 Ctrl+S），会一路沉到这里
    if (e->type() == QEvent::KeyPress) {
        // Handle Global Shortcuts...
    }

    return false; // 兜底任务本身通常不消费绘图事件
}
