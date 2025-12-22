#include "user_bar_widget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace {
QString formatLoginTime(const QDateTime& dt) {
    return dt.toString("yyyy-MM-dd HH:mm:ss");
}
} // namespace

UserBarWidget::UserBarWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(12);

    usernameLabel_ = new QLabel(tr("User: -"), this);
    loginTimeLabel_ = new QLabel(tr("Login Time: -"), this);

    switchUserBtn_ = new QPushButton(tr("Switch User"), this);
    logoutBtn_ = new QPushButton(tr("Logout"), this);
    settingsBtn_ = new QPushButton(tr("Env / Connection"), this);

    layout->addWidget(usernameLabel_);
    layout->addWidget(loginTimeLabel_);
    layout->addStretch();
    layout->addWidget(settingsBtn_);
    layout->addWidget(switchUserBtn_);
    layout->addWidget(logoutBtn_);

    connect(switchUserBtn_, &QPushButton::clicked, this, &UserBarWidget::switchUserClicked);
    connect(logoutBtn_, &QPushButton::clicked, this, &UserBarWidget::logoutClicked);
    connect(settingsBtn_, &QPushButton::clicked, this, &UserBarWidget::settingsClicked);

    setUsername(QString());
    setLoginTime(QDateTime::currentDateTime());
}

void UserBarWidget::setUsername(const QString& username) {
    const QString name = username.isEmpty() ? tr("-") : username;
    usernameLabel_->setText(tr("User: %1").arg(name));
}

void UserBarWidget::setLoginTime(const QDateTime& dt) {
    loginTimeLabel_->setText(tr("Login Time: %1").arg(formatLoginTime(dt)));
}
