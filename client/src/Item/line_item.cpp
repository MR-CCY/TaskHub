// line_item.cpp
#include "line_item.h"

#include <QDebug>
#include <QPainter>
#include <QLineF>

#include "commands/command.h"
#include "rect_item.h"

// 计算从矩形中心指向目标点的射线与矩形边的交点（scene 坐标）
static QPointF projectToRectEdge(const QRectF& rect, const QPointF& target) {
    QPointF center = rect.center();
    QLineF ray(center, target);
    const QPointF tl = rect.topLeft();
    const QPointF tr = rect.topRight();
    const QPointF br = rect.bottomRight();
    const QPointF bl = rect.bottomLeft();
    const QLineF edges[] = { QLineF(tl, tr), QLineF(tr, br), QLineF(br, bl), QLineF(bl, tl) };
    QPointF inter;
    for (const auto& edge : edges) {
        if (ray.intersects(edge, &inter) == QLineF::BoundedIntersection) {
            return inter;
        }
    }
    return target; // 退化情况直接返回目标
}

LineItem::LineItem(RectItem* start, RectItem* end, QGraphicsItem* parent)
    : BaseItem(Kind::Edge, parent), start_(start), end_(end)
{
    setZValue(-1); // 线在底层
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
    path_ = QPainterPath();

    // 锚点取矩形边缘，避免穿过节点中心
    QRectF startRectScene = start_->mapRectToScene(start_->boundingRect());
    QRectF endRectScene   = end_->mapRectToScene(end_->boundingRect());
    QPointF sceneP1 = projectToRectEdge(startRectScene, endRectScene.center());
    QPointF sceneP2 = projectToRectEdge(endRectScene,   startRectScene.center());
    QPointF p1 = mapFromScene(sceneP1);
    QPointF p2 = mapFromScene(sceneP2);
    
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
