#include "factory/edge_factory.h"
#include "item/edge_item.h"

BaseItem* EdgeFactory::create(const QMap<QString, QVariant>& initProps) {
    const QString from = initProps.value("from").toString();
    const QString to   = initProps.value("to").toString();
    auto* e = new EdgeItem(from, to);
    e->setFactoryUri(uri());
    return e;
}