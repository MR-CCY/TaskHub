#include "delete_task.h"

#include "commands/command.h"
#include "view/canvasscene.h"
#include "commands/undostack.h"
#include "Item/base_item.h"
#include "Item/line_item.h"
#include "Item/rect_item.h"
DeleteTask::DeleteTask(CanvasScene* scene, UndoStack* undo, QObject* parent)
    : Task(200, parent), scene_(scene), undo_(undo) {}

bool DeleteTask::dispatch(QEvent* e) {
    Q_UNUSED(e);
    return false;
}

void DeleteTask::execute() {
    if (!scene_ || !undo_) {
        removeSelf();
        return;
    }
    QSet<QGraphicsItem*> items;
    for (QGraphicsItem* item : scene_->selectedItems()) {
        items.insert(item);
    }
    if (items.isEmpty()) {
        removeSelf();
        return;
    }
    auto* cmd = new DeleteCommand(scene_, items);
    undo_->push(cmd);
    removeSelf();
}
