#pragma once
#include "task.h"
#include "Item/node_type.h"
class QGraphicsItem;
class CanvasScene;
class UndoStack;
class CanvasView;
class RectItem;

// 导入 JSON 任务：一次性执行并自销毁，Level 200
class ImportTask : public Task {
public:
    ImportTask(CanvasScene* scene, UndoStack* undo, CanvasView* view, QWidget* parentWidget = nullptr, QObject* parent = nullptr);

    bool dispatch(QEvent* e) override;
    void execute();

private:
    bool parseJson(const QString& path, QJsonObject& rootOut);
    bool validateTasks(const QJsonObject& root, QJsonArray& tasksOut);
     // jsonObjectToStringMap, typeFromExec removed if not used, wait. 
     // jsonObjectToStringMap is local utility.
     // typeFromExec is local utility.
     // Let's remove them if we removed impl. I removed implementation.
    bool resolveConflicts(const QJsonArray& tasks, const QHash<QString, RectItem*>& existing, QHash<QString, RectItem*>& idToNode);

    CanvasScene* scene_;
    UndoStack* undo_;
    CanvasView* view_;
    QWidget* parentWidget_;
};
