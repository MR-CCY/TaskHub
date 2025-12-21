#include "task.h"
#include "task_manager.h"
#include "view/canvasbench.h"
Task::Task(int level, QObject* parent)
    : QObject(parent), level_(level) {
      auto bench = qobject_cast<CanvasBench*>(parent);
      if(bench){
        setView(bench->view());
      }
    }

Task::~Task() = default;

void Task::removeSelf()
{
    if (mgr_) {
        mgr_->removeTask(this);
    }
}

bool Task::handleBaseKeyEvent(QEvent* e)
{
    if (e->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(e);
        if (ke->key() == Qt::Key_Escape) {
            removeSelf();
            return true;
        }
    }
    return false;
}
