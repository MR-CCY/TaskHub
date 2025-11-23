#include "login_dialog.h"
#include <QGridLayout>

LoginDialog::LoginDialog(QWidget *parent)
    :QDialog(parent)
{
    setWindowTitle("Login");
    setModal(true);
    QLineEdit* m_editBaseUrl = new QLineEdit("",this);
    QLabel *usernameLabel = new QLabel("Username:", this);
    QLabel *passwordLabel = new QLabel("Password:", this);

    QLineEdit *usernameEdit = new QLineEdit(this);
    QLineEdit *passwordEdit = new QLineEdit(this);
    passwordEdit->setEchoMode(QLineEdit::Password);

    QPushButton *loginButton = new QPushButton("登陆", this);
    QPushButton *cancelButton = new QPushButton("取消", this);

    QGridLayout *layout = new QGridLayout(this);
    layout->addWidget(usernameLabel, 0, 0);
    layout->addWidget(usernameEdit, 0, 1);
    layout->addWidget(passwordLabel, 1, 0);
    layout->addWidget(passwordEdit, 1, 1);
    layout->addWidget(loginButton, 2, 0);
    layout->addWidget(cancelButton, 2, 1);

    setLayout(layout);

    connect(loginButton, &QPushButton::clicked, this, &LoginDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &LoginDialog::reject);
}