#include "user_bar_widget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace {
QString formatLoginTime(const QDateTime &dt) {
  return dt.toString("yyyy-MM-dd HH:mm:ss");
}
} // namespace

UserBarWidget::UserBarWidget(QWidget *parent) : QWidget(parent) {
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(8, 4, 8, 4);
  layout->setSpacing(12);

  usernameLabel_ = new QLabel(tr("用户: -"), this);
  loginTimeLabel_ = new QLabel(tr("登录时间: -"), this);

  logoutBtn_ = new QPushButton(tr("退出"), this);

  layout->addWidget(usernameLabel_);
  layout->addWidget(loginTimeLabel_);
  layout->addStretch();
  layout->addWidget(logoutBtn_);

  connect(logoutBtn_, &QPushButton::clicked, this,
          &UserBarWidget::logoutClicked);

  setUsername(QString());
  setLoginTime(QDateTime::currentDateTime());
}

void UserBarWidget::setUsername(const QString &username) {
  const QString name = username.isEmpty() ? tr("-") : username;
  usernameLabel_->setText(tr("用户: %1").arg(name));
}

void UserBarWidget::setLoginTime(const QDateTime &dt) {
  loginTimeLabel_->setText(tr("登录时间: %1").arg(formatLoginTime(dt)));
}
