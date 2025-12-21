#include "task_manager.h"
#include "task.h"

#include <algorithm>

TaskManager::TaskManager(QObject* parent)
    : QObject(parent)
{
}

TaskManager::~TaskManager()
{
    clear();
}

void TaskManager::clear()
{
    // 栈里的 Task 通常 parent=bench 或 parent=this，会自动析构；
    // 但为了保险，这里只断开引用，不强行 delete（避免 double delete）
    stack_.clear();
    top_ = nullptr;
    emit stackChanged();
}

void TaskManager::rebuildNextChain()
{
    // stack_: [bottom ... top]
    // next 链：top->next 指向下一个（更底层）
    for (int i = static_cast<int>(stack_.size()) - 1; i >= 0; --i) {
        Task* cur = stack_[i];
        Task* nxt = (i - 1 >= 0) ? stack_[i - 1] : nullptr;
        cur->next_ = nxt;
        cur->_setManager(this);
    }
    top_ = stack_.empty() ? nullptr : stack_.back();
}

void TaskManager::popTop()
{
    if (stack_.empty()) return;
    Task* t = stack_.back();
    stack_.pop_back();

    // 不 delete：让 QObject parent 体系负责生命周期
    // 但要断开链与 manager
    t->next_ = nullptr;
    t->_setManager(nullptr);

    rebuildNextChain();
}

void TaskManager::push(Task* t)
{
    if (!t) return;

    // ===== level 规则：高等级入栈 -> 弹出所有比它低等级的 task =====
    // 解释：栈顶一般是“临时工具/模式”，level 更低；底层常驻更高。
    // 当一个更“重”的 task 进来，意味着要切换到更底层/更强控制态，
    // 所以把上面的临时任务都清掉。
    const int newLevel = t->level();

    while (!stack_.empty()) {
        Task* top = stack_.back();
        if (top->level() <= newLevel) {
            popTop(); // 弹掉更低等级（更轻）的
        } else {
            break;
        }
    }

    // 避免重复 push 同一个对象
    if (std::find(stack_.begin(), stack_.end(), t) != stack_.end()) {
        rebuildNextChain();
        emit stackChanged();
        return;
    }

    stack_.push_back(t);
    rebuildNextChain();
    emit stackChanged();
}

bool TaskManager::dispatch(QEvent* e)
{
    if (!e) return false;
    if (!top_) return false;

    // 每次派发前清掉 accepted 状态，避免上一次残留（有些事件不会复用，但保险）
    e->setAccepted(false);

    // 从栈顶开始下沉
    Task* cur = top_;
    while (cur) {
        // 先处理基类的通用事件（如 Esc 自销毁），MainTask 会覆盖返回 false
        if (cur->handleBaseKeyEvent(e)) {
            e->accept();
            return true;
        }
        const bool handled = cur->dispatch(e);

        // 你们公司口径：event 被接收（accepted）就不再下沉
        if (handled || e->isAccepted()) {
            e->accept();
            return true;
        }
        cur = cur->next();
    }

    return false;
}

void TaskManager::removeTask(Task* t)
{
    if (!t) return;

    // 找到 task 在 stack_ 的位置
    auto it = std::find(stack_.begin(), stack_.end(), t);
    if (it == stack_.end()) return;

    // ===== 堆栈解旋语义（你们公司描述的 removeSelf 机制）=====
    // 当一个临时任务结束时，通常要“回到正确层级”。
    // 这里采取：删除它，并把它上方（更轻、更临时）的任务一并弹出。
    // 因为 stack_ back 是顶，所以：
    //   [bottom ... t ... top]  -> 删除 t，并 pop 直到 t 以上都没了
    const int idx = static_cast<int>(std::distance(stack_.begin(), it));
    const int topIdx = static_cast<int>(stack_.size()) - 1;

    // 先把顶上更轻的都弹掉
    for (int i = topIdx; i > idx; --i) {
        popTop();
    }

    // 现在 t 应该在栈顶
    if (!stack_.empty() && stack_.back() == t) {
        popTop();
    } else {
        // 如果不在顶（理论上不会发生），就直接 erase
        stack_.erase(std::remove(stack_.begin(), stack_.end(), t), stack_.end());
        rebuildNextChain();
    }

    emit stackChanged();
}
