// core/app_context.h
#pragma once
#include <QString>

class AppContext {
public:
    static AppContext& instance();

    void setBaseUrl(const QString& url);
    QString baseUrl() const;

    void setToken(const QString& token);
    QString token() const;

    bool isLoggedIn() const;

private:
    AppContext() = default;
    QString m_baseUrl;   // 例如: http://localhost:8082
    QString m_token;     // Bearer 后面的那串
};