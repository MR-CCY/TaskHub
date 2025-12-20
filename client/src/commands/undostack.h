// UndoStack.h
#pragma once
#include <QObject>
#include <memory>
#include <vector>
#include "command.h"

class UndoStack : public QObject {
    Q_OBJECT
public:
    explicit UndoStack(QObject* parent=nullptr);

    bool push(std::unique_ptr<ICommand> cmd);
    bool canUndo() const;
    bool canRedo() const;
    
    QString undoText() const; // 当前可 undo 的命令名（等价于 QUndoStack::undoText）
    QString redoText() const; // 当前可 redo 的命令名
public slots:
    void undo();
    void redo();

signals:
    void changed();

private:
    std::vector<std::unique_ptr<ICommand>> cmds_;
    std::size_t index_ = 0;
};
