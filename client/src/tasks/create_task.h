// create_task.h
#pragma once
#include <QString>

#include "task.h"
#include "Item/node_type.h"

class CreateTask : public Task {
public:
    explicit CreateTask(QObject* parent = nullptr);
    CreateTask(NodeType type, const QString& templateId, QObject* parent = nullptr);
    bool dispatch(QEvent* e) override;
private:
    bool useFactory_ = false;
    NodeType nodeType_ = NodeType::Local;
    QString templateId_;
};
