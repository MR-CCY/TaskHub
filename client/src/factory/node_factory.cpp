#include "factory/node_factory.h"
#include "item/node_item.h"

BaseItem* NodeFactory::create(const QMap<QString, QVariant>& initProps) {
    const QString id = initProps.value("id").toString();
    auto* n = new NodeItem(id);
    n->setFactoryUri(uri());
    // pos 在 Scene addItem 后 setPos 更合适（也可以先 set）
    return n;
}