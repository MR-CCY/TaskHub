#include "arrow_line_item.h"

#include <QPainter>
#include <QtMath>



/**
 * @brief 构造一个带箭头的连线项
 * @param start 连线起点关联的矩形项
 * @param end 连线终点关联的矩形项
 * @param parent 父图元项
 */
ArrowLineItem::ArrowLineItem(RectItem* start, RectItem* end, QGraphicsItem* parent)
    : LineItem(start, end, parent)
{}

/**
 * @brief 获取连线项的边界矩形
 * @return 包含箭头空间的边界矩形
 */
QRectF ArrowLineItem::boundingRect() const {
    // 额外给箭头留足边距，避免移动时残影（文字旋转后实际包络更大）
    constexpr qreal pad = 48.0;
    return LineItem::boundingRect().adjusted(-pad, -pad, pad, pad);
}

/**
 * @brief 绘制带箭头的连线
 * @param painter 用于绘制的QPainter对象
 * @param option 图形项样式选项
 * @param widget 绘制所在的widget
 */
void ArrowLineItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) {
    painter->setRenderHint(QPainter::Antialiasing);

    const QPainterPath& path = currentPath();
    const qreal totalLen = path.length();
    
    if (totalLen < 1.0) return;

    // 获取起点和终点的颜色
    QColor startColor = startItem() ? startItem()->headerColor() : QColor(124, 251, 250);
    QColor endColor = endItem() ? endItem()->headerColor() : QColor(124, 251, 250);

    // 将路径端点转换到场景坐标系来创建渐变
    QPointF pathStartLocal = path.pointAtPercent(0.0);
    QPointF pathEndLocal = path.pointAtPercent(1.0);
    QPointF pathStartScene = mapToScene(pathStartLocal);
    QPointF pathEndScene = mapToScene(pathEndLocal);
    
    // 在场景坐标系中创建渐变，然后映射回 item 坐标
    QLinearGradient gradient(mapFromScene(pathStartScene), mapFromScene(pathEndScene));
    gradient.setColorAt(0.0, startColor);
    gradient.setColorAt(1.0, endColor);

    // 使用渐变色绘制线条
    QPen pen;
    pen.setBrush(gradient);
    pen.setWidth(20);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    
    if (isSelected()) {
        pen.setBrush(Qt::blue);
        pen.setStyle(Qt::DashLine);
    }
    
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path);

    // 只在非选中状态下绘制箭头斑纹
    if (!isSelected()) {
        // 绘制白色侧向V字形箭头斑纹（>形状，类似道路标线）
        painter->setPen(Qt::NoPen);
        painter->setBrush(Qt::white);

        const qreal arrowWidth = 16.0;   // 箭头总高度（垂直方向）
        const qreal arrowLength = 12.0;  // 箭头长度（水平方向）
        const qreal lineWidth = 5.0;     // 每条边的宽度
        const qreal spacing = 25.0;      // 箭头之间的间距

        for (qreal d = spacing; d < totalLen - spacing; d += spacing) {
            qreal t = path.percentAtLength(d);
            QPointF pt = path.pointAtPercent(t);
            qreal angleDeg = path.angleAtPercent(t);

            painter->save();
            painter->translate(pt);
            painter->rotate(-angleDeg); // 对齐路径方向

            // 绘制侧向V字形箭头（>形状）
            // 计算三个端点
            QPointF tip(arrowLength/2, 0);                    // 右侧尖端
            QPointF topLeft(-arrowLength/2, -arrowWidth/2);  // 左上
            QPointF bottomLeft(-arrowLength/2, arrowWidth/2); // 左下
            
            // 使用 stroker 创建圆角线条
            QPainterPathStroker stroker;
            stroker.setWidth(lineWidth);
            stroker.setCapStyle(Qt::RoundCap);
            stroker.setJoinStyle(Qt::RoundJoin);
            
            // 绘制上半边（左上到右尖端）
            QPainterPath topLine;
            topLine.moveTo(topLeft);
            topLine.lineTo(tip);
            QPainterPath topStroke = stroker.createStroke(topLine);
            painter->drawPath(topStroke);
            
            // 绘制下半边（右尖端到左下）
            QPainterPath bottomLine;
            bottomLine.moveTo(tip);
            bottomLine.lineTo(bottomLeft);
            QPainterPath bottomStroke = stroker.createStroke(bottomLine);
            painter->drawPath(bottomStroke);

            painter->restore();
        }
    }
}
