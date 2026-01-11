// line_item.h
#pragma once
#include <QPainterPath>
#include <QPointF>

#include "base_item.h"
#include "rect_item.h"
class RectItem;

class LineItem : public BaseItem {
public:
    // start, end: 连接的两个矩形
    LineItem(RectItem* start, RectItem* end, QGraphicsItem* parent = nullptr);
    ~LineItem() override;

    int type() const override { return static_cast<int>(Kind::Edge); }
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    QPainterPath shape() const override;

    // 当 RectItem 移动时调用此函数更新端点
    void trackNodes();

    // 调整控制点（用于改变线形）
    void setControlOffset(const QPointF& offset);
    QPointF controlOffset() const { return controlOffset_; }

    // 获取端点对象
    RectItem* startItem() const { return start_; }
    RectItem* endItem() const { return end_; }

    // 生产命令的接口实现
    void execCreateCmd(bool canUndo) override;
    void execMoveCmd(const QPointF& newPos, bool canUndo) override; // 这里的 Move 指调整形状

protected:
    const QPainterPath& currentPath() const { return path_; }

private:
    RectItem* start_;
    RectItem* end_;
    QPointF controlOffset_; // 贝塞尔控制点相对于中点的偏移量
    mutable QPainterPath path_; // 缓存路径
};
