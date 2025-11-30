#include "task_list_model.h"
#include <QBrush>
#include <QJsonObject>
TaskListModel::TaskListModel(QObject *parent)
    : QAbstractTableModel(parent)
{}

void TaskListModel::setTasks(const QList<TaskItem> &tasks)
{
    beginResetModel();
    m_tasks = tasks;
    endResetModel();
}

TaskItem TaskListModel::taskAt(int row) const
{
    return m_tasks.at(row);
}

int TaskListModel::rowCount(const QModelIndex &parent) const
{
    return m_tasks.size();
}

int TaskListModel::columnCount(const QModelIndex &parent) const
{
    return  9;
}

QVariant TaskListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    const auto& t = m_tasks[index.row()];

    if (role == Qt::ForegroundRole && index.column() == 2) { // status 列
        if (t.status == "success")
            return QBrush(Qt::darkGreen);
        if (t.status == "failed")
            return QBrush(Qt::red);
        if (t.status == "running")
            return QBrush(Qt::blue);
        if (t.status == "pending")
            return QBrush(Qt::gray);
        return {};
    }

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0: return t.id;
        case 1: return t.name;
        case 2: return t.status;      // pending / running / success / failed
        case 3: return t.cmd;
        case 4: return t.createTime;
        case 5: return t.updateTime;
        case 6: return t.exitCode;
        case 7: return t.output;
        case 8: return t.errorMsg;
        }
    }

    return {};
}

QVariant TaskListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return QVariant();

    switch (section) {
    case 0:
        return "ID";
    case 1:
        return "任务名称";
    case 2:
        return "任务状态";
    case 3:
        return "任务命令";
    case 4:
        return "创建时间";
    case 5:
        return "更新时间";
    case 6:
        return "退出码";
    case 7:
        return "输出";
    case 8:
        return "错误信息";
    default:
        return QVariant();
    }
}

int TaskListModel::rowById(int id) const
{
    for (int i = 0; i < m_tasks.size(); ++i) {
        if (m_tasks[i].id == id) {
            return i;
        }
    }
    return -1;
}

void TaskListModel::upsertFromJson(const QJsonObject &obj)
{
    TaskItem item;
    item.id=obj["id"].toInt();
    item.type = obj["type"].toInt();
    item.name=obj["name"].toString();
    item.status=obj["status"].toString();
    item.cmd=obj["params"].toObject()["cmd"].toString();
    item.createTime=obj["create_time"].toString();
    item.updateTime=obj["update_time"].toString();
    item.exitCode=obj["exit_code"].toInt();
    item.output=obj["last_output"].toString();
    item.errorMsg=obj["last_error"].toString();

    
    int row = rowById(item.id);
    if (row < 0) {
        // 新任务：插入
        const int insertRow = m_tasks.size();
        beginInsertRows(QModelIndex(), insertRow, insertRow);
        m_tasks.push_back(item);
        endInsertRows();
    } else {
        // 已存在：更新
        m_tasks[row] = item;
        QModelIndex left  = index(row, 0);
        QModelIndex right = index(row, columnCount() - 1);
        emit dataChanged(left, right);
    }
}
