// undostack.h
#pragma once
#include <QUndoStack>
#include <QObject>

class UndoStack : public QObject {
    Q_OBJECT
public:
    explicit UndoStack(QObject* parent = nullptr);
    
    void push(QUndoCommand* cmd);
    void undo();
    void redo();
    
    // 核心：自动宏管理
    void beginMacro(const QString& text);
    void endMacro();

    QUndoStack* internalStack() const { return stack_; }

private:
    QUndoStack* stack_;
    int macroCount_ = 0; // 防止嵌套宏调用出错
};