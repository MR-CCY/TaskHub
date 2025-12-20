#include "login_dialog.h"
#include <QGridLayout>
#include <QJsonObject>
#include "app_context.h"
#include <QSettings>
#include <QMessageBox>

LoginDialog::LoginDialog(QWidget *parent)
    :QDialog(parent)
{
    setWindowTitle("Login");

    

    // setModal(true);
    QLabel *usernameLabel = new QLabel("用户名:", this);
    QLabel *passwordLabel = new QLabel("密码:", this);
    m_editUsername = new QLineEdit(this);
    m_editPassword = new QLineEdit(this);
    m_editPassword->setEchoMode(QLineEdit::Password);

    QPushButton *loginButton = new QPushButton("登陆", this);
    QPushButton *cancelButton = new QPushButton("取消", this);

    QGridLayout *layout = new QGridLayout(this);
    layout->addWidget(usernameLabel,1,0);
    layout->addWidget(m_editUsername,1,1);
    layout->addWidget(passwordLabel,2,0);
    layout->addWidget(m_editPassword,2,1);
    layout->addWidget(cancelButton, 3, 0);
    layout->addWidget(loginButton, 3, 1);

    QSettings s("TaskHub", "Client");
    m_editUsername->setText(s.value("username", "admin").toString());
    setLayout(layout);

    m_appApiClient=new ApiClient();
    m_appApiClient->setBaseUrl("http://localhost:8082");

    connect(loginButton, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    connect(cancelButton, &QPushButton::clicked, this, &LoginDialog::reject);
    connect(m_appApiClient, &ApiClient::loginFailed, [this](int httpStatus, const QString& message) {
        QMessageBox::warning(this, "登录失败", "登录失败："+message);
    });
    connect(m_appApiClient, &ApiClient::loginOk, [this](const QString& token, const QString& username) {
        // m_appApiClient->setToken(token);
        AppContext::instance().setToken(token);
        // m_appApiClient->setUsername(username);
        // m_appApiClient->getHealth();
        // m_appApiClient->getInfo();
        this->accept();
        return;
    });

}
void LoginDialog::onLoginClicked(){
    m_appApiClient->login(m_editUsername->text(),m_editPassword->text());
}
