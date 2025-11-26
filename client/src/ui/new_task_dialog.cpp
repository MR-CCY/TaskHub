#include "new_task_dialog.h"
#include <QPushButton>
NewTaskDialog::NewTaskDialog(QWidget *parent)
:QDialog(parent)
{
    ui = new Ui::NewTaskDialog();
    ui->setupUi(this);
}

NewTaskDialog::~NewTaskDialog()
{
}

QString NewTaskDialog::taskName() const
{
    return  ui->lineEdit->text();
}

QString NewTaskDialog::cmd() const
{
    return ui->lineEdit_2->text();
}
