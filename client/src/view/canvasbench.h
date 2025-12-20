#pragma once
#include <QWidget>

class QToolBar;
class QAction;
class QVBoxLayout;

// 前置声明 (Forward Declarations)
class CanvasScene;
class CanvasView;
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
 
public slots:
    void undo();
    void redo();
    void startConnectMode(); // 槽函数
    void deleteSelected();   // 槽函数
private:
    void buildCore(); // 初始化 STOC 核心对象
    void buildUi();   // 初始化工具栏和布局
    void wireUi();    // 绑定信号槽
    void startSelectMode();
private:
    // ===== 核心对象 (STOC) =====
    CanvasScene* scene_ = nullptr;
    CanvasView* view_ = nullptr;
    UndoStack* undoStack_ = nullptr;
    TaskManager* taskMgr_ = nullptr;

    // ===== 默认任务 =====
    SelectTask* selectTask_ = nullptr; // 常驻栈底

    // ===== UI 组件 =====
    QToolBar* toolbar_ = nullptr;
    QAction* actCreateRect_ = nullptr;
    QAction* actUndo_ = nullptr;
    QAction* actRedo_ = nullptr;
    QAction* actConnect_ = nullptr;
    QAction* actDelete_ = nullptr;
};