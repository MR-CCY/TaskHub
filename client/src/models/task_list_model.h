#pragma once
#include <QAbstractTableModel>
#include "models/task_item.h"
#include <QList>
class TaskListModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit TaskListModel(QObject* parent = nullptr);

    void setTasks(const QList<TaskItem>& tasks);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation, int role) const override;

private:
    QList<TaskItem> m_tasks;
};