#pragma once

#include <QString>
#include <QDateTime>
#include <QUuid>

namespace taskhub::utils {

inline QString generateRunId() {
    return QString::number(QDateTime::currentMSecsSinceEpoch()) + "_" +
           QUuid::createUuid().toString().remove('{').remove('}').remove('-').mid(0, 6);
}

} // namespace taskhub::utils
