#pragma once
#include <QObject>
#include <QUrl>
#include <QJsonObject>
class QWebSocket;

class WsClient : public QObject {
    Q_OBJECT
public:
    explicit WsClient(QObject* parent = nullptr);
    static  WsClient* instance();
    ~WsClient();
    void setToken(const QString& token);
    void connectTo(const QUrl& wsUrl);
    void close();
    
    void subscribeTaskLogs(const QString& taskId, const QString& runId = {});
    void unsubscribeTaskLogs(const QString& taskId, const QString& runId = {});
    void subscribeTaskEvents(const QString& taskId, const QString& runId = {});
    void unsubscribeTaskEvents(const QString& taskId, const QString& runId = {});
signals:
    void connected();
    void authed();
    void messageReceived(const QJsonObject& obj);
    void error(const QString& msg);
    void closed();

private:
    void sendJson(const QJsonObject& obj);

private:
    QWebSocket* ws_ = nullptr;
    QString token_;
    bool authed_ = false;
    QSet<QPair<QString,QString>> subscribedLogs_;
    QSet<QPair<QString,QString>> subscribedEvents_;
};
