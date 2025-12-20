// create_task.h
#pragma once
#include "task.h"

class CreateTask : public Task {
public:
    explicit CreateTask(QObject* parent = nullptr);
    bool dispatch(QEvent* e) override;
};