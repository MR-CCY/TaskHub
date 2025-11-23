// core/http_client.h
#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <functional>

class HttpClient : public QObject {
    Q_OBJECT
public:
    explicit HttpClient(QObject* parent = nullptr);

    using JsonCallback = std::function<void(bool ok, const QJsonObject& resp)>;

    void postJson(const QString& path, const QJsonObject& body, JsonCallback cb);
    void getJson(const QString& path, JsonCallback cb);

private:
    QNetworkAccessManager m_mgr;

    void applyAuthHeader(QNetworkRequest& req);
};