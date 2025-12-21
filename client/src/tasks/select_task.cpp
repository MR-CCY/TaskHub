#include "select_task.h"
#include "move_task.h"       // 引入新任务
#include "view/canvasview.h"
#include "tasks/task_manager.h"
#include <QMouseEvent>

SelectTask::SelectTask(QObject* parent)
    : Task(200, parent) // Level 20：比 MoveTask(10) 重，作为底座
{
}

bool SelectTask::dispatch(QEvent* e)
{
    // SelectTask 只关心“按下”
    // 移动和松开都交给 MoveTask 处理了，所以这里不需要监听它们
    if (e->type() == QEvent::MouseButtonPress) {
        return handlePress(e);
    }
    return false;
}

bool SelectTask::handlePress(QEvent* e) {
    auto* me = static_cast<QMouseEvent*>(e);
    if (me->button() != Qt::LeftButton) return false;
    
    QPointF scenePos = view()->mapToScene(me->pos());
    
    // 1. 命中检测
    auto* item = view()->scene()->itemAt(scenePos, QTransform());

    if (item) {
        // 2. 选中图元
        view()->scene()->clearSelection();
        item->setSelected(true);
        
        // 3. 【核心变化】检测到击中，立刻压入 MoveTask
        // 无论用户是想“点击”还是“拖拽”，MoveTask 都能处理：
        //   - 如果不移动就松开 -> MoveTask 里面 hasMoved_ 为 false，不产生命令，直接退出。
        //   - 如果移动 -> MoveTask 处理拖拽。
        auto* moveTask = new MoveTask(item, scenePos, manager());
        moveTask->setView(view()); // 别忘了注入 View
        manager()->push(moveTask);
        
        return true;
    } else {
        // 点空了：清空选择
        view()->scene()->clearSelection();
    }
    return false;
}