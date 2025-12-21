#include "straight_line_task.h"

#include "Item/line_item.h"
#include <QDebug>

StraightLineTask::StraightLineTask(LineItem* line, QObject* parent)
    : Task(1, parent), line_(line) {}

bool StraightLineTask::dispatch(QEvent* /*e*/) {
    // 不需要事件循环，任务只负责一次性操作
    return false;
}

void StraightLineTask::straighten() {
    if (!line_) {
        removeSelf();
        return;
    }
    // 通过命令通路，便于撤销
    line_->execMoveCmd(QPointF(0, 0), true);
    line_->update();
    removeSelf();
}
