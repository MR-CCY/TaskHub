#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include "net/api_client.h"
class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(QWidget* parent = nullptr);

private slots:
    void onLoginClicked();

private:
    QLineEdit* m_editUsername;
    QLineEdit* m_editPassword;
    ApiClient* m_appApiClient;
};
