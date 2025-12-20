#pragma once
#include <QObject>
#include <QEvent>

class TaskManager;
class CanvasView;

// 交互态 / 工具 / 模式：不是后台任务
class Task : public QObject {
    Q_OBJECT
public:
    explicit Task(int level, QObject* parent = nullptr);
    ~Task() override;

    int level() const { return level_; }

    // 事件处理：返回 true 表示“我处理并接收了”，停止下沉
    virtual bool dispatch(QEvent* e) = 0;

    // 任务栈链表（从栈顶往下沉）
    Task* next() const { return next_; }

    // 由 TaskManager 设置
    void _setManager(TaskManager* mgr) { mgr_ = mgr; }
    TaskManager* manager() const { return mgr_; }

    // 退出自己（以及按规则弹出更“轻”的任务）
    void removeSelf();

    // 可选：给 View 做辅助（你也可以不需要）
    void setView(CanvasView* v) { view_ = v; }
    CanvasView* view() const { return view_; }

private:
    friend class TaskManager;
    int level_ = 0;
    Task* next_ = nullptr;
    TaskManager* mgr_ = nullptr;
    CanvasView* view_ = nullptr;
};
