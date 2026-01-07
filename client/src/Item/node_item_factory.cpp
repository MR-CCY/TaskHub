#include "node_item_factory.h"

#include <QDebug>
#include <QGraphicsItem>

#include "http_rect_item.h"
#include "local_rect_item.h"
#include "remote_rect_item.h"
#include "shell_rect_item.h"
#include "dag_rect_item.h"
#include "template_rect_item.h"

RectItem* NodeItemFactory::createNode(NodeType type, const QRectF& rect, QGraphicsItem* parent) {
    switch (type) {
    case NodeType::Shell:
        return new ShellRectItem(rect, parent);
    case NodeType::Http:
        return new HttpRectItem(rect, parent);
    case NodeType::Remote:
        return new RemoteRectItem(rect, parent);
    case NodeType::Local:
        return new LocalRectItem(rect, parent);
    case NodeType::Dag:
        return new DagRectItem(rect, parent);
    case NodeType::Template:
        return new TemplateRectItem(rect, parent);
    default:
        qWarning() << "NodeItemFactory::createNode received unknown NodeType:" << static_cast<int>(type);
        return nullptr;
    }
}

NodeType NodeItemFactory::fromString(const QString& value) {
    const QString key = value.trimmed().toLower();
    if (key == QStringLiteral("shell"))  return NodeType::Shell;
    if (key == QStringLiteral("http"))   return NodeType::Http;
    if (key == QStringLiteral("remote")) return NodeType::Remote;
    if (key == QStringLiteral("local"))  return NodeType::Local;
    if (key == QStringLiteral("dag"))    return NodeType::Dag;
    if (key == QStringLiteral("template")) return NodeType::Template;

    qWarning() << "NodeItemFactory::fromString unknown value:" << value << "- defaulting to Local";
    return NodeType::Local;
}

QString NodeItemFactory::toString(NodeType type) {
    switch (type) {
    case NodeType::Shell:  return QStringLiteral("Shell");
    case NodeType::Http:   return QStringLiteral("Http");
    case NodeType::Remote: return QStringLiteral("Remote");
    case NodeType::Local:  return QStringLiteral("Local");
    case NodeType::Dag:    return QStringLiteral("Dag");
    case NodeType::Template: return QStringLiteral("Template");
    }
    qWarning() << "NodeItemFactory::toString unknown NodeType:" << static_cast<int>(type) << "- returning empty string";
    return {};
}

bool NodeItemFactory::isContainerType(NodeType type) {
    return type == NodeType::Remote || type == NodeType::Dag || type == NodeType::Template;
}
