#pragma once
#include <QPointF>

#include "task.h"

class SelectTask : public Task {
    Q_OBJECT
public:
    // Level = 10，常驻底层
    explicit SelectTask(QObject* parent = nullptr);
    bool dispatch(QEvent* e) override;

private:
    bool handlePress(QEvent* e);
};
