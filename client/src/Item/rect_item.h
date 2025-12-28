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
    RectItem(const QRectF& rect, QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter* p, const QStyleOptionGraphicsItem* opt, QWidget* w) override;


    // 实现接口：生产创建命令
    void execCreateCmd(bool canUndo) override;
    // 重写 itemChange 监听移动
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

    // 批量设置 TaskConfig 字段（仅覆盖传入的键）
    void setTaskConfig(const QVariantMap& cfg);

    void addLine(LineItem* line);
    void removeLine(LineItem* line);
    // 占位，暂时空实现
    void execMoveCmd(const QPointF&, bool) override {}
    QSet<LineItem*> lines(){return lines_;};

      // ---- 给子类覆写的“类型外观接口” ----
      virtual QString typeLabel() const;     // 例如 >_ / HTTP / SSH / fx
      virtual QColor headerColor() const;    // 头部色条
      virtual QString summaryText() const;   // 底部摘要（从 props_ 拼出来）
      // 属性访问（支持 exec_params.* / metadata.*）
      QVariant propByKeyPath(const QString& keyPath) const;
      void setPropByKeyPath(const QString& keyPath, const QVariant& v);
      const QString& nodeId() const { return nodeId_; }
      void setNodeId(const QString& id) { nodeId_ = id; }
      void setStatusOverlay(const QColor& c) { statusColor_ = c; update(); }
      QColor statusOverlay() const { return statusColor_; }
      void setStatusLabel(const QString& s) { statusLabel_ = s; update(); }
      QString statusLabel() const { return statusLabel_; }
      
      // 可选：提供 rect 访问（子类一般不需要直接碰 rect_）
      QRectF rect() const { return rect_; }
protected:
      // 外观参数统一放这里（你后续统一风格只改这里）
      qreal radius_ = 10.0;
      qreal headerH_ = 22.0;
      qreal pad_ = 8.0;
private:
    QRectF rect_;
    QSet<LineItem*> lines_;
    QString nodeId_;   // 逻辑 Node ID（编辑态唯一）
    QColor statusColor_;
    QString statusLabel_;
};
