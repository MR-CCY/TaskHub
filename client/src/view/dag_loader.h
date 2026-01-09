#pragma once

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QHash>

class CanvasScene;
class UndoStack;
class RectItem;
class QGraphicsItem;

class DagLoader {
public:
    // 递归加载入口
    // tasks: 当前层的任务数组
    // scene: 场景
    // undo: 撤销栈
    // parentNode: 父节点（用于递归）
    // preExisting: 预先存在的节点（用于 ImportTask 冲突解决复用）
    // error: 错误信息
    static void loadLevel(const QJsonArray& tasks, 
                          CanvasScene* scene, 
                          UndoStack* undo, 
                          QGraphicsItem* parentNode,
                          const QHash<QString, RectItem*>& preExisting,
                          QHash<QString, RectItem*>& outCreatedNodes,
                          const QString& pathPrefix = QString());

private:
   static bool inflateDagJson(RectItem* node, const QString& dagJson, CanvasScene* scene, UndoStack* undo, QHash<QString, RectItem*>& outCreatedNodes, const QString& pathPrefix);
   static bool inflateDagObject(RectItem* node, const QJsonObject& dagObj, CanvasScene* scene, UndoStack* undo, QHash<QString, RectItem*>& outCreatedNodes, const QString& pathPrefix);
};
