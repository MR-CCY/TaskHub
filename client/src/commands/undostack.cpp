// UndoStack.cpp
#include "undostack.h"
#include "command.h"

/**
 * @brief 构造一个新的撤销栈对象
 * 
 * @param parent 父对象指针，用于Qt的对象树管理
 */
UndoStack::UndoStack(QObject* parent) : QObject(parent) {}

/**
 * @brief 将新命令推入栈中并执行
 * 
 * 此方法会先清除当前索引之后的所有命令（如果有），然后执行新命令并将其加入栈中。
 * 如果命令执行失败，则不会被添加到栈中。
 * 
 * @param cmd 要推入的命令对象的唯一指针
 * @return true 命令成功执行并添加到栈中
 * @return false 命令为空或执行失败
 */
bool UndoStack::push(std::unique_ptr<ICommand> cmd) {
    if (!cmd) return false;
    if (index_ < cmds_.size()) {
        cmds_.erase(cmds_.begin() + static_cast<long>(index_), cmds_.end());
    }
    if (!cmd->redo()) return false;
    cmds_.push_back(std::move(cmd));
    index_ = cmds_.size();
    emit changed();
    return true;
}

/**
 * @brief 检查是否可以执行撤销操作
 * 
 * @return true 当前位置不在栈底部，可以撤销
 * @return false 当前位置在栈底部，无法撤销
 */
bool UndoStack::canUndo() const { return index_ > 0; }

/**
 * @brief 检查是否可以执行重做操作
 * 
 * @return true 当前位置不在栈顶部，可以重做
 * @return false 当前位置在栈顶部，无法重做
 */
bool UndoStack::canRedo() const { return index_ < cmds_.size(); }

/**
 * @brief 执行撤销操作
 * 
 * 撤销当前命令并将索引向前移动一位。
 * 如果不能撤销则直接返回。
 */
void UndoStack::undo() {
    if (!canUndo()) return;
    cmds_[index_ - 1]->undo();
    --index_;
    emit changed();
}

/**
 * @brief 执行重做操作
 * 
 * 重做下一个命令并将索引向后移动一位。
 * 如果不能重做则直接返回。
 */
void UndoStack::redo() {
    if (!canRedo()) return;
    cmds_[index_]->redo();
    ++index_;
    emit changed();
}
QString UndoStack::undoText() const {
    if (!canUndo()) return {};
    // index_ 指向“下一条 redo 的位置”，所以可 undo 的是 index_-1
    return cmds_[index_ - 1]->name();
}

QString UndoStack::redoText() const {
    if (!canRedo()) return {};
    // 可 redo 的是 index_
    return cmds_[index_]->name();
}