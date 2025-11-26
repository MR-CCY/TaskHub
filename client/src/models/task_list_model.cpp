#include "task_list_model.h"
#include <QBrush>
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
