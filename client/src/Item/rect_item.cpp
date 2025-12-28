#include "rect_item.h"

#include "line_item.h"

static QString ellipsize(const QString& s, int maxChars) {
    if (s.size() <= maxChars) return s;
    return s.left(maxChars - 1) + "…";
}

void RectItem::execCreateCmd(bool canUndo)
{
    // 注意：这里需要 sceneContext() 和 undoStack() 已经被注入
    // 在 Task 里创建 Item 后，必须调用 attachContext
    if (!sceneCtx()) return;

    auto* cmd = new CreateCommand(sceneCtx(), this);
    cmd->pushTo(undo());
}
QVariant RectItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionHasChanged) {
        // 当我移动时，通知所有连线更新
        for (auto* line : lines_) {
            line->trackNodes();
        }
    }
    return BaseItem::itemChange(change, value);
}

void RectItem::addLine(LineItem* line) {
    lines_.insert(line);
}

void RectItem::removeLine(LineItem* line) {
    lines_.remove(line);
}
RectItem::RectItem(const QRectF &rect, QGraphicsItem *parent)
: BaseItem(Kind::Node, parent), rect_(rect) 
{
    setFlag(QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemIsMovable);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges);
    setCacheMode(QGraphicsItem::NoCache); // 避免移动时的残影
    props_["name"] = "Node";
    props_["enabled"] = true;

}

QRectF RectItem::boundingRect() const
{
    // 给描边/端口/状态标签留边距，避免移动时重绘不全
    const qreal margin = std::max<qreal>(12.0, headerH_);
    return rect_.adjusted(-margin, -margin - headerH_, margin, margin);
}

QPainterPath RectItem::shape() const {
    QPainterPath path;
    path.addRoundedRect(rect_, radius_, radius_);
    return path;
}

void RectItem::paint(QPainter* p, const QStyleOptionGraphicsItem* opt, QWidget*) {
    p->setRenderHint(QPainter::Antialiasing, true);

    const bool selected = (opt && (opt->state & QStyle::State_Selected));
    const bool enabled  = props_.value("enabled", true).toBool();

    // 背景 / 边框
    QColor bg = enabled ? QColor(250, 250, 250) : QColor(235, 235, 235);
    QColor border = selected ? QColor(30, 144, 255) : QColor(60, 60, 60);
    if (statusColor_.isValid()) {
        border = statusColor_;
    }

    p->setPen(QPen(border, selected ? 2.5 : 3.0));
    p->setBrush(bg);
    p->drawRoundedRect(rect_, radius_, radius_);

    // Header 色条
    QRectF headerR(rect_.left(), rect_.top(), rect_.width(), headerH_);
    p->setPen(Qt::NoPen);
    p->setBrush(headerColor());
    p->drawRoundedRect(headerR, radius_, radius_);
    // 覆盖下半圆角，避免 header 下缘出现“弧形断层”
    p->drawRect(QRectF(headerR.left(), headerR.top() + radius_, headerR.width(), headerR.height() - radius_));

    // Header 文本：typeLabel + name
    QString name = props_.value("name", "Node").toString();
    const QString id= props_.value("id", "").toString();
    QString title = QString("%1  %2 ID:%3").arg(typeLabel(), name,id);

    p->setPen(QColor(255, 255, 255));
    QFont f = p->font();
    f.setBold(true);
    p->setFont(f);
    p->drawText(headerR.adjusted(pad_, 0, -pad_, 0),
                Qt::AlignVCenter | Qt::AlignLeft,
                ellipsize(title, 32));

    // 摘要
    p->setPen(QColor(40, 40, 40));
    QFont f2 = p->font();
    f2.setBold(false);
    p->setFont(f2);

    QRectF bodyR = rect_.adjusted(pad_, headerH_ + pad_, -pad_, -pad_);
    p->drawText(bodyR, Qt::TextWordWrap, ellipsize(summaryText(), 140));

    // 端口点（左入右出，先预留）
    p->setBrush(QColor(80, 80, 80));
    p->setPen(Qt::NoPen);
    const qreal r = 4.0;
    QPointF inP(rect_.left(),  rect_.center().y());
    QPointF outP(rect_.right(), rect_.center().y());
    p->drawEllipse(inP + QPointF(-r, -r), r, r);
    p->drawEllipse(outP + QPointF(-r, -r), r, r);

    // Status label at top-left outside
    if (statusColor_.isValid() && !statusLabel_.isEmpty()) {
        const QString text = statusLabel_;
        QFont sf = p->font();
        sf.setBold(true);
        p->setFont(sf);
        QRectF tagRect(rect_.left() - 4, rect_.top() - headerH_, rect_.width() * 0.6, headerH_);
        p->setPen(QPen(Qt::black, 1.2));
        p->setBrush(statusColor_);
        p->drawRoundedRect(tagRect, 6, 6);
        p->setPen(QPen(Qt::white, 1.2));
        p->drawText(tagRect.adjusted(6, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
    }
}
QString RectItem::typeLabel() const { return "NODE"; }
QColor  RectItem::headerColor() const { return QColor(80, 80, 80); }
QString RectItem::summaryText() const { return ""; }

void RectItem::setTaskConfig(const QVariantMap& cfg) {
    // 仅覆盖存在的键，保持类型一致
    static const char* keys[] = {
        "id", "name", "exec_type", "exec_command", "exec_params",
        "timeout_ms", "retry_count", "retry_delay_ms", "retry_exp_backoff",
        "priority", "queue", "capture_output", "metadata"
    };
    for (const auto key : keys) {
        if (cfg.contains(key)) {
            props_[key] = cfg.value(key);
        }
    }
}

QVariant RectItem::propByKeyPath(const QString& keyPath) const {
    if (keyPath.startsWith("exec_params.")) {
        QVariantMap m = props_.value("exec_params").toMap();
        QString sub = keyPath.mid(QString("exec_params.").size());
        return m.value(sub);
    }
    if (keyPath.startsWith("metadata.")) {
        QVariantMap m = props_.value("metadata").toMap();
        QString sub = keyPath.mid(QString("metadata.").size());
        return m.value(sub);
    }
    return props_.value(keyPath);
}

void RectItem::setPropByKeyPath(const QString& keyPath, const QVariant& v) {
    if (keyPath.startsWith("exec_params.")) {
        QVariantMap m = props_.value("exec_params").toMap();
        QString sub = keyPath.mid(QString("exec_params.").size());
        m[sub] = v;
        props_["exec_params"] = m;
        return;
    }
    if (keyPath.startsWith("metadata.")) {
        QVariantMap m = props_.value("metadata").toMap();
        QString sub = keyPath.mid(QString("metadata.").size());
        m[sub] = v;
        props_["metadata"] = m;
        return;
    }
    props_[keyPath] = v;
}
