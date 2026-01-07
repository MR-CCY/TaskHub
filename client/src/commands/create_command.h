#pragma once
#include "command.h"
#include <QGraphicsItem>
#include <QGraphicsScene>

class CreateCommand : public BaseCommand {
public:
    CreateCommand(QGraphicsScene* scene, QGraphicsItem* item, QUndoCommand* parent = nullptr);
    ~CreateCommand() override;
    void execute() override;
    void unExecute() override;

private:
    void adjustParentContainer();

    QGraphicsScene* scene_;
    QGraphicsItem* item_;
    QGraphicsItem* parent_ = nullptr;
    QPointF pos_;
};
