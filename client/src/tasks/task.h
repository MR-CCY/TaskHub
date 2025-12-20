#pragma once
#include <QEvent>

class ITask {
public:
    virtual ~ITask() = default;
    virtual bool handleEvent(QEvent* e) = 0;
};