#include "base_item.h"

BaseItem::BaseItem(Kind k, QGraphicsItem* parent)
    : QGraphicsObject(parent), kind_(k) {}

void BaseItem::attachContext(QGraphicsScene* s, GraphModel* m, UndoStack* u) {
    scene_ = s;
    model_ = m;
    undo_  = u;
}

QVariant BaseItem::propByKeyPath(const QString& keyPath) const {
    if (!keyPath.contains('.')) {
        return props_.value(keyPath);
    }

    QStringList parts = keyPath.split('.');
    return getNestedValue(props_, parts, 0);
}

void BaseItem::setPropByKeyPath(const QString& keyPath, const QVariant& v) {
    if (!keyPath.contains('.')) {
        props_[keyPath] = v;
        return;
    }

    QStringList parts = keyPath.split('.');
    props_ = setNestedValue(props_, parts, 0, v);
}

QVariantMap BaseItem::setNestedValue(const QVariantMap &source, const QStringList &path, int index, const QVariant &value) const
{
    if (index >= path.size()) return source;
    
    QVariantMap result = source;
    QString currentKey = path[index];
    
    if (index == path.size() - 1) {
        result[currentKey] = value;
    } else {
        QVariantMap subMap;
        if (result.contains(currentKey) && result[currentKey].canConvert<QVariantMap>()) {
            subMap = result[currentKey].toMap();
        }
        result[currentKey] = setNestedValue(subMap, path, index + 1, value);
    }
    
    return result;
}

QVariant BaseItem::getNestedValue(const QVariantMap &source, const QStringList &path, int index) const
{
    if (index >= path.size()) return QVariant();
    
    QString currentKey = path[index];
    if (!source.contains(currentKey)) {
        return QVariant();
    }
    
    if (index == path.size() - 1) {
        return source.value(currentKey);
    }
    
    QVariant subValue = source.value(currentKey);
    if (subValue.canConvert<QVariantMap>()) {
        return getNestedValue(subValue.toMap(), path, index + 1);
    }
    
    return QVariant();
}
