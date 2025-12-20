#pragma once
#include <QObject>
#include <vector>

class Task;

// 任务栈：带等级（level）规则的交互控制器
// - level 越高（数值越大）越“重”，越应该在下面（更底层、常驻）
// - push 高等级任务时：弹出所有比它低等级的任务（更轻、更临时）
// - 事件从栈顶向下沉：某个 Task 返回 true 或 event->isAccepted() 就停止
class TaskManager : public QObject {
    Q_OBJECT
public:
    explicit TaskManager(QObject* parent = nullptr);
    ~TaskManager() override;

    Task* top() const { return top_; }

    // 压栈：遵守 level 规则
    void push(Task* t);

    // 派发事件：从栈顶向下沉
    bool dispatch(QEvent* e);

    // 移除某个 Task（供 Task::removeSelf 调用）
    void removeTask(Task* t);

    // 可选：清理所有任务（比如关闭画布时）
    void clear();

signals:
    void stackChanged();

private:
    // 弹出栈顶一个 task
    void popTop();

    // 断开并重建 next 链（用于下沉）
    void rebuildNextChain();

private:
    Task* top_ = nullptr;

    // 我们用 vector 当栈：back() 是栈顶，front() 是栈底
    std::vector<Task*> stack_;
};