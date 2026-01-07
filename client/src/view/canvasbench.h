#pragma once
#include <QWidget>
#include "canvasview.h"

enum class NodeType;
class RectItem;
class LineItem;

// 前置声明 (Forward Declarations)
class CanvasScene;
class UndoStack;
class TaskManager;
class SelectTask;

class CanvasBench : public QWidget {
    Q_OBJECT
public:
    explicit CanvasBench(QWidget* parent = nullptr);
    ~CanvasBench() override;

    // 对外接口：切换到“创建矩形”模式
    void startCreateRectMode(); 
    void startCreateShellMode();
    void startCreateHttpMode();
    void startCreateRemoteMode();
    void startCreateLocalMode();
    // void startZoomTask();
    CanvasView* view() const { return view_; }
 
public slots:
    void undo();
    void redo();
    void startConnectMode(); // 槽函数
    void deleteSelected();   // 槽函数
    void exportJson();       // 导出 JSON
    void importJson();       // 导入 JSON
    void editSelectedNode(); // 编辑节点属性
    void layoutDag();        // DAG 自动布局

protected:
    CanvasScene* scene() const { return scene_; }
    UndoStack* undoStack() const { return undoStack_; }
    TaskManager* taskManager() const { return taskMgr_; }

    void startSelectMode();
    void startCreateNodeMode(NodeType type);
private:
    void buildCore(); // 初始化 STOC 核心对象
    void startZoomTask();
private:
    // ===== 核心对象 (STOC) =====
    CanvasScene* scene_ = nullptr;
    CanvasView* view_ = nullptr;
    UndoStack* undoStack_ = nullptr;
    TaskManager* taskMgr_ = nullptr;

    // ===== 默认任务 =====
    SelectTask* selectTask_ = nullptr; // 常驻栈底
};
