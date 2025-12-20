// rect_item.h
#pragma once
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QWidget>
#include <QSet>

#include "base_item.h"
#include "commands/create_command.h"

class LineItem;

class RectItem : public BaseItem {
public:
    RectItem(const QRectF& rect, QGraphicsItem* parent = nullptr)
        : BaseItem(Kind::Node, parent), rect_(rect) 
    {
        setFlag(QGraphicsItem::ItemIsSelectable);
        setFlag(QGraphicsItem::ItemIsMovable);
        setFlag(QGraphicsItem::ItemSendsGeometryChanges);
    }

    QRectF boundingRect() const override { return rect_; }
    
    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override {
        painter->setBrush(Qt::blue);
        painter->drawRect(rect_);
    }

    // 实现接口：生产创建命令
    void execCreateCmd(bool canUndo) override {
        // 注意：这里需要 sceneContext() 和 undoStack() 已经被注入
        // 在 Task 里创建 Item 后，必须调用 attachContext
        if (!sceneCtx()) return;

        auto* cmd = new CreateCommand(sceneCtx(), this);
        cmd->pushTo(undo());
    }
    // 重写 itemChange 监听移动
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

    void addLine(LineItem* line);
    void removeLine(LineItem* line);
    // 占位，暂时空实现
    void execMoveCmd(const QPointF&, bool) override {}
    QSet<LineItem*> lines(){return lines_;};

private:
    QRectF rect_;
    QSet<LineItem*> lines_;
};
