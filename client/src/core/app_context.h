// core/app_context.h
#pragma once
#include <QString>
#include <QDateTime>
class AppContext {
public:
    static AppContext& instance();

    void setBaseUrl(const QString& url);
    QString baseUrl() const;

    void setToken(const QString& token);
    QString token() const;

    bool isLoggedIn() const;
    QString username() const;
    void setUsername(const QString& username);
    QString loginTime() const;
    void setLoginTime();

private:
    AppContext() = default;
    QString m_baseUrl;   // 例如: http://localhost:8082
    QString m_token;     // Bearer 后面的那串
    QString m_username;
    QDateTime m_loginTime;
};