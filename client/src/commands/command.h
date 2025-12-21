#pragma once
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QList>
#include <QPointF>
#include <QUndoCommand>

#include "commands/undostack.h"

class RectItem;
class LineItem;

class BaseCommand : public QUndoCommand {
public:
    explicit BaseCommand(const QString& text, QUndoCommand* parent = nullptr);
    virtual void execute() = 0;
    virtual void unExecute() = 0;
    void redo() override;
    void undo() override;
    void pushTo(UndoStack* stack);
};

class CreateConnectionCommand : public BaseCommand {
public:
    CreateConnectionCommand(QGraphicsScene* scene, LineItem* line, QUndoCommand* parent = nullptr);
    ~CreateConnectionCommand() override;
    void execute() override;
    void unExecute() override;

private:
    QGraphicsScene* scene_;
    LineItem* line_;
};

class MoveRectCommand : public BaseCommand {
public:
    MoveRectCommand(RectItem* item, QPointF oldPos, QPointF newPos);
    void execute() override;
    void unExecute() override;

private:
    RectItem* item_;
    QPointF oldPos_, newPos_;
};

class AdjustLineCommand : public BaseCommand {
public:
    AdjustLineCommand(LineItem* item, QPointF oldOff, QPointF newOff);
    void execute() override;
    void unExecute() override;

private:
    LineItem* item_;
    QPointF oldOff_, newOff_;
};

class DeleteCommand : public BaseCommand {
public:
    DeleteCommand(QGraphicsScene* scene, const QSet<QGraphicsItem*>& items);
    ~DeleteCommand() override;
    void execute() override;
    void unExecute() override;

private:
    QGraphicsScene* scene_;
    QSet<QGraphicsItem*> items_;
};
