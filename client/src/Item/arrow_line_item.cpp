#include "arrow_line_item.h"

#include <QPainter>
#include <QtMath>

namespace {
/**
 * @brief 绘制沿路径方向居中的">"符号
 * @param painter 用于绘制的QPainter对象
 * @param pos 符号绘制位置
 * @param angleDeg 符号旋转角度（度）
 * @param font 字体设置
 * @param color 符号颜色
 */
void drawArrowGlyph(QPainter* painter, const QPointF& pos, qreal angleDeg, const QFont& font, const QColor& color) {
    painter->save();
    painter->translate(pos);
    painter->rotate(-angleDeg); // align glyph with path direction
    painter->setPen(color);
    painter->setBrush(color);
    painter->setFont(font);
    QFontMetrics fm(font);
    const QString g(">");
    const QRect bbox = fm.boundingRect(g);
    QRectF rect(-bbox.width() / 2.0, -bbox.height() / 2.0, bbox.width(), bbox.height());
    painter->drawText(rect, Qt::AlignCenter, g);
    painter->restore();
}
}

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

    const QColor lineColor(124, 251, 250); // 背景线色（亮青）
    QPen pen(lineColor, 20); // 稍微加粗
    if (isSelected()) {
        pen.setColor(Qt::blue);
        pen.setStyle(Qt::DashLine);
    }
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);

    const QPainterPath& path = currentPath();
    painter->drawPath(path);

    // 在路径中心绘制文字箭头花纹 >>>>
    const qreal totalLen = path.length();
    if (totalLen > 1.0) {
        QFont glyphFont = painter->font();
        glyphFont.setPointSizeF(glyphFont.pointSizeF() + 8); // 字体更大，箭头更粗
        QFontMetrics fm(glyphFont);
        const QString glyph(">"); 
        const int adv = fm.horizontalAdvance(glyph);
        const qreal spacing = adv + 4.0; // 控制密度
        const QColor glyphColor(31, 31, 31); // 深色箭头

        for (qreal d = spacing; d < totalLen - spacing; d += spacing) {
            qreal t = path.percentAtLength(d);
            QPointF pt = path.pointAtPercent(t);
            qreal angle = path.angleAtPercent(t);
            drawArrowGlyph(painter, pt, angle, glyphFont, glyphColor);
        }
    }
}
