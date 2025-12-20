#include "task.h"
#include "task_manager.h"

Task::Task(int level, QObject* parent)
    : QObject(parent), level_(level) {}

Task::~Task() = default;

void Task::removeSelf()
{
    if (mgr_) {
        mgr_->removeTask(this);
    }
}