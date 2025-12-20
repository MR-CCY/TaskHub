#pragma once
#include <QString>

class ICommand {
public:
    virtual ~ICommand() = default;
    virtual QString name() const = 0;
    virtual bool redo() = 0;
    virtual void undo() = 0;
};