#pragma once
#include <QWidget>
#include <QDateTime>

class QLabel;
class QPushButton;

// Simple top bar showing current user and login time with switch/logout actions.
class UserBarWidget : public QWidget {
    Q_OBJECT
public:
    explicit UserBarWidget(QWidget* parent = nullptr);

    void setUsername(const QString& username);
    void setLoginTime(const QDateTime& dt);

signals:
    void switchUserClicked();
    void logoutClicked();
    void settingsClicked();

private:
    QLabel* usernameLabel_ = nullptr;
    QLabel* loginTimeLabel_ = nullptr;
    QPushButton* switchUserBtn_ = nullptr;
    QPushButton* logoutBtn_ = nullptr;
    QPushButton* settingsBtn_ = nullptr;
};
