#pragma once
#include "factory/item_factory.h"

class EdgeFactory : public ItemFactory {
public:
    QString uri() const override { return "factory.edge.bezier"; }
    BaseItem* create(const QMap<QString, QVariant>& initProps) override;
};