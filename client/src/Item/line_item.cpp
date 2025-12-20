// line_item.cpp
#include "line_item.h"

#include <QDebug>
#include <QPainter>

#include "commands/command.h"
#include "rect_item.h"

LineItem::LineItem(RectItem* start, RectItem* end, QGraphicsItem* parent)
    : BaseItem(Kind::Edge, parent), start_(start), end_(end)
{
    setZValue(0); // 线在底层
    setFlag(QGraphicsItem::ItemIsSelectable);
    
    // 告诉 RectItem：我有连着你 (简单的观察者模式)
    if(start_) start_->addLine(this);
    if(end_) end_->addLine(this);
    
    trackNodes();
}

LineItem::~LineItem() {
    // 析构时解绑（防止野指针）
    if(start_) start_->removeLine(this);
    if(end_) end_->removeLine(this);
}

void LineItem::trackNodes() {
    if (!start_ || !end_) return;
    prepareGeometryChange();
    // 核心：线本身不移动位置(0,0)，而是不断重绘路径
    path_ = QPainterPath();
    QPointF p1 = mapFromItem(start_, start_->boundingRect().center());
    QPointF p2 = mapFromItem(end_, end_->boundingRect().center());
    
    path_.moveTo(p1);
    
    // 二次贝塞尔控制点 = 中点 + 偏移量
    QPointF center = (p1 + p2) / 2;
    QPointF c1 = center + controlOffset_;
    
    path_.quadTo(c1, p2);
    update();
}

void LineItem::setControlOffset(const QPointF& offset) {
    controlOffset_ = offset;
    trackNodes();
}

QRectF LineItem::boundingRect() const {
    // 外包框需要包含线宽和控制点，这里稍微放大一点防止残影
    return path_.boundingRect().adjusted(-10, -10, 10, 10);
}

QPainterPath LineItem::shape() const {
    // 用于碰撞检测：生成路径的轮廓
    QPainterPathStroker stroker;
    stroker.setWidth(10); // 更容易点中
    return stroker.createStroke(path_);
}

void LineItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing);
    QPen pen(Qt::black, 2);
    if (isSelected()) {
        pen.setColor(Qt::blue);
        pen.setStyle(Qt::DashLine);
    }
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path_);
}

void LineItem::execCreateCmd(bool canUndo) {
    if (!sceneCtx()) return;
    auto* cmd = new CreateConnectionCommand(sceneCtx(), this);
    cmd->pushTo(undo());
}

void LineItem::execMoveCmd(const QPointF& offset, bool canUndo) {
    // 这里的 move 实际上是调整控制点
    auto* cmd = new AdjustLineCommand(this, controlOffset_, offset); // offset 此时当作新值用
    cmd->pushTo(undo());
}
