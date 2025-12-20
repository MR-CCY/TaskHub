#pragma once
#include <QString>
#include <QMap>
#include <QVariant>

class BaseItem;

class ItemFactory {
public:
    virtual ~ItemFactory() = default;
    virtual QString uri() const = 0;

    // initProps: id/type/pos/from/to...
    virtual BaseItem* create(const QMap<QString, QVariant>& initProps) = 0;
};