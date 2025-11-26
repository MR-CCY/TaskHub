#pragma once
#include <QString>
struct TaskItem {
    qint64      id;
    QString     name;
    int         type;
    QString     status;
    QString     createTime;
    QString     updateTime;
    QString     cmd;          // 从 params.cmd 中取
    int         exitCode;
    QString     output;
    QString     errorMsg;
};