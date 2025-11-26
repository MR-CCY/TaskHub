#include "taskdetaildialog.h"

TaskDetailDialog::TaskDetailDialog(QWidget *parent):
    QDialog(parent),
    ui(new Ui::TaskDetailDialogUi())
{
    ui->setupUi(this);

}


void TaskDetailDialog::setTaskDetail(const TaskItem &item)
{
    ui->lineEdit_name->setText(item.name);
    ui->lineEdit_status->setText(item.status);
    ui->lineEdit_cmd->setText(item.cmd);
    ui->textEdit_out->setPlainText(item.output.isEmpty() ? item.errorMsg : item.output);

}
