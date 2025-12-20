// undostack.cpp
#include "undostack.h"

UndoStack::UndoStack(QObject* parent) : QObject(parent) {
    stack_ = new QUndoStack(this);
}

void UndoStack::push(QUndoCommand* cmd) {
    stack_->push(cmd);
}

void UndoStack::undo() { stack_->undo(); }
void UndoStack::redo() { stack_->redo(); }

void UndoStack::beginMacro(const QString& text) {
    if (macroCount_ == 0) {
        stack_->beginMacro(text);
    }
    macroCount_++;
}

void UndoStack::endMacro() {
    macroCount_--;
    if (macroCount_ <= 0) {
        stack_->endMacro();
        macroCount_ = 0;
    }
}