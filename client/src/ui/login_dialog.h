#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include "http_client.h"
#include <QPushButton>
class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(QWidget* parent = nullptr);

private slots:
    void onLoginClicked();

private:
    QLineEdit* m_editBaseUrl;
    QLineEdit* m_editUsername;
    QLineEdit* m_editPassword;
    QLabel*    m_labelError;
    QPushButton* m_button_login;
    HttpClient m_http;
};