#include "login_dialog.h"
#include <QGridLayout>
#include <QJsonObject>
#include <app_context.h>

LoginDialog::LoginDialog(QWidget *parent)
    :QDialog(parent)
{
    setWindowTitle("Login");
    // setModal(true);
    QLabel *baseTip = new QLabel("后端地址:", this);
    QLabel *usernameLabel = new QLabel("用户名:", this);
    QLabel *passwordLabel = new QLabel("密码:", this);
    m_editBaseUrl = new QLineEdit("http://localhost:8082",this);
    m_labelError = new QLabel("", this);
    m_editUsername = new QLineEdit(this);
    m_editPassword = new QLineEdit(this);
    m_editPassword->setEchoMode(QLineEdit::Password);

    QPushButton *loginButton = new QPushButton("登陆", this);
    QPushButton *cancelButton = new QPushButton("取消", this);

    QGridLayout *layout = new QGridLayout(this);
    layout->addWidget(baseTip, 0, 0);
    layout->addWidget(m_editBaseUrl, 0, 1);
    layout->addWidget(usernameLabel,1,0);
    layout->addWidget(m_editUsername,1,1);
    layout->addWidget(passwordLabel,2,0);
    layout->addWidget(m_editPassword,2,1);
    layout->addWidget(cancelButton, 3, 0);
    layout->addWidget(loginButton, 3, 1);
    layout->addWidget(m_labelError, 4, 0);
    

    setLayout(layout);

    connect(loginButton, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    connect(cancelButton, &QPushButton::clicked, this, &LoginDialog::reject);
    m_http=new HttpClient(this);
}
void LoginDialog::onLoginClicked(){
    AppContext &appcontext = AppContext::instance();
    QString baseUrl = m_editBaseUrl->text();
    appcontext.setBaseUrl(baseUrl);
    QJsonObject body;
    body["username"]=m_editUsername->text();
    body["password"]=m_editPassword->text();
    
    m_http->postJson("/api/login", body, [this](bool ok, QJsonObject resp) { 
        if(!ok||resp["code"] != 0){
            m_labelError->setText("登录失败："+resp["message"].toString());
            return;
        }
        auto data=resp["data"].toObject();
        QString token=data["token"].toString();
        AppContext &appcontext = AppContext::instance();
        appcontext.setToken(token);
        accept();
        return;
    });
}
