#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
class QUrl;
class QNetworkAccessManager;
class QNetworkRequest;


class ApiClient : public QObject {
    Q_OBJECT
public:
    explicit ApiClient(QObject* parent = nullptr);

    void setBaseUrl(const QString& baseUrl);   // e.g. http://127.0.0.1:8082
    QString baseUrl() const { return baseUrl_; }

    void setToken(const QString& token);       // jwt-like from /api/login
    QString token() const { return token_; }

    // Q0-3 required APIs
    void login(const QString& username, const QString& password);
    void getHealth();
    void getInfo();
    void runDagAsync(const QJsonObject& body);

signals:
    void unauthorized();
    // login
    void loginOk(const QString& token, const QString& username);
    void loginFailed(int httpStatus, const QString& message);

    // health/info
    void healthOk(const QJsonObject& data);
    void infoOk(const QJsonObject& data);
    void runDagAsyncOk(const QString& runId, const QJsonArray& taskIds);

    // generic
    void requestFailed(const QString& apiName, int httpStatus, const QString& message);
    void rawJson(const QString& apiName, const QJsonObject& obj); // optional: for ConsoleDock

private:
    struct ParsedEnvelope {
        bool ok = false;
        QString message;
        QJsonValue data; 
        QJsonObject root;  
    };

    QNetworkRequest makeRequest(const QString& path) const;
    void getJson(const QString& apiName, const QString& path);
    void postJson(const QString& apiName, const QString& path, const QJsonObject& body);

    static bool parseJsonObject(const QByteArray& bytes, QJsonObject& out, QString& err);
    static ParsedEnvelope parseEnvelope(const QJsonObject& root);

private:
    QNetworkAccessManager* nam_ = nullptr;
    QString baseUrl_;
    QString token_;
};
