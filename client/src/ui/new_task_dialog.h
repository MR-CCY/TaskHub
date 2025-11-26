#pragma once
#include "ui_new_task_dialog.h"
#include <QDialog>
class NewTaskDialog : public QDialog{
    Q_OBJECT
public:
    explicit NewTaskDialog(QWidget* parent = nullptr);
    ~NewTaskDialog();
    QString taskName() const;
    QString cmd() const;
private:
    Ui::NewTaskDialog* ui;
};