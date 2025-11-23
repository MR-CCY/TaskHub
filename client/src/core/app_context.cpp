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
