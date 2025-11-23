#include "http_client.h"
#include "app_context.h"
#include <QNetworkRequest>
#include <QNetworkReply>
HttpClient::HttpClient(QObject *parent):
    QObject(parent)
{
}

void HttpClient::postJson(const QString &path, const QJsonObject &body, JsonCallback cb)
{
    QNetworkRequest request(QUrl(AppContext::instance().baseUrl() + path));
    applyAuthHeader(request);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkReply *reply = manager->post(request, QJsonDocument(body).toJson());

    connect(reply, &QNetworkReply::finished, this, [reply, cb]() {
        QByteArray response_data = reply->readAll();
        QJsonDocument jsonResponse = QJsonDocument::fromJson(response_data);
        if (reply->error() == QNetworkReply::NoError) {
            cb(true, jsonResponse.object());
        } else {
            cb(false, jsonResponse.object());
        }
        reply->deleteLater();
    });
}

void HttpClient::getJson(const QString &path, JsonCallback cb)
{
    QNetworkRequest request(QUrl(AppContext::instance().baseUrl() + path));
    applyAuthHeader(request);

    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkReply *reply = manager->get(request);

    connect(reply, &QNetworkReply::finished, this, [reply, cb]() {
        QByteArray response_data = reply->readAll();
        QJsonDocument jsonResponse = QJsonDocument::fromJson(response_data);
        if (reply->error() == QNetworkReply::NoError) {
            cb(true, jsonResponse.object());
        } else {
            cb(false, jsonResponse.object());
        }
        reply->deleteLater();
    });
}

void HttpClient::applyAuthHeader(QNetworkRequest &req)
{
    if (AppContext::instance().isLoggedIn()) {
        QString authHeader = "Bearer " + AppContext::instance().token();
        req.setRawHeader("Authorization", authHeader.toUtf8()); 
    }
}
