#include "task_list_model.h"

TaskListModel::TaskListModel(QObject *parent)
    : QAbstractTableModel(parent)
{}

void TaskListModel::setTasks(const QList<TaskItem> &tasks)
{
    beginResetModel();
    m_tasks = tasks;
    endResetModel();
}

int TaskListModel::rowCount(const QModelIndex &parent) const
{
    return m_tasks.size();
}

int TaskListModel::columnCount(const QModelIndex &parent) const
{
    return 5;
}

QVariant TaskListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole)
        return QVariant();

    const TaskItem &item = m_tasks.at(index.row());
    switch (index.column()) {
    case 0:
        return item.id;
    case 1:
        return item.name;
    case 2:
        return item.type;
    case 3:
        return item.status;
    case 4:
        return item.createTime;
    default:
        return QVariant();
    }
}

QVariant TaskListModel::headerData(int section, Qt::Orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    switch (section) {
    case 0:
        return "ID";
    case 1:
        return "任务名称";
    case 2:
        return "任务类型";
    case 3:
        return "任务状态";
    case 4:
        return "创建时间";
    default:
        return QVariant();
    }
}
