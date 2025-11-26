#pragma once
#include <QDialog>
#include "ui_taskdetaildialog.h"
#include "models/task_item.h"

class TaskDetailDialog : public QDialog {
    Q_OBJECT
public:
    explicit TaskDetailDialog(QWidget* parent = nullptr);  
    void setTaskDetail(const TaskItem& item);
private:
    Ui::TaskDetailDialogUi* ui=nullptr;
};