#include "app_context.h"
AppContext &AppContext::instance()
{
    // TODO: 在此处插入 return 语句
    static AppContext instance;
    return instance;
}

void AppContext::setBaseUrl(const QString &url)
{
    m_baseUrl = url;
}

QString AppContext::baseUrl() const
{
    return m_baseUrl;
}

void AppContext::setToken(const QString &token)
{
    m_token = token;
}

QString AppContext::token() const
{
    return m_token;
}

bool AppContext::isLoggedIn() const
{
    return !m_token.isEmpty();
}

QString AppContext::username() const
{
    return m_username;
}

void AppContext::setUsername(const QString &username)
{
    m_username = username;
}

QString AppContext::loginTime() const
{
    return m_loginTime.toString("yyyy-MM-dd HH:mm:ss");
}

void AppContext::setLoginTime()
{
    m_loginTime = QDateTime::currentDateTime();
}
