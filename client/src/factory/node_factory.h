#pragma once
#include "factory/item_factory.h"

class NodeFactory : public ItemFactory {
public:
    QString uri() const override { return "factory.node.rect"; }
    BaseItem* create(const QMap<QString, QVariant>& initProps) override;
};