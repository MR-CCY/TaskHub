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
    QUrl url(AppContext::instance().baseUrl() + path);
    QNetworkRequest request(url);
    applyAuthHeader(request);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    qDebug() << "req url valid?" << url.isValid() << url;
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkReply *reply = manager->post(request, QJsonDocument(body).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply, cb]() {
        handleReply(reply, cb);
    });
}

void HttpClient::getJson(const QString &path, JsonCallback cb)
{
    QNetworkRequest request(QUrl(AppContext::instance().baseUrl() + path));
    applyAuthHeader(request);

    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkReply *reply = manager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, cb]() {
        handleReply(reply, cb);
    });
}

void HttpClient::handleReply(QNetworkReply *reply, JsonCallback cb)
{
    QByteArray data = reply->readAll();
    int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    QJsonObject obj;
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isObject())
        obj = doc.object();

    if (httpStatus == 401 || obj["code"].toInt() == 401) {
        emit unauthorized();
        cb(false, obj);
        return;
    }

    bool ok = (httpStatus >= 200 && httpStatus < 300);
    cb(ok, obj);
    reply->deleteLater();
}

void HttpClient::applyAuthHeader(QNetworkRequest &req)
{
    if (AppContext::instance().isLoggedIn()) {
        QString authHeader = "Bearer " + AppContext::instance().token();
        req.setRawHeader("Authorization", authHeader.toUtf8()); 
    }
}
