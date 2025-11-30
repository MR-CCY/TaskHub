#pragma once
#include <QString>
struct TaskItem {
    qint64      id = 0;
    QString     name;
    int         type = 0;
    QString     status;
    QString     createTime;
    QString     updateTime;
    QString     cmd;          // 从 params.cmd 中取
    int         exitCode = 0;
    QString     output;
    QString     errorMsg;
};