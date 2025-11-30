
#pragma once
#include <QObject>
#include <QWebSocket>
#include <QJsonObject>

class TaskWsClient : public QObject
{
    Q_OBJECT
public:
    explicit TaskWsClient(QObject *parent = nullptr);

    // 连接到 WebSocket 服务器，例如 ws://127.0.0.1:8090/ws/tasks
    void connectToServer(const QUrl &url);

    // 断开连接
    void disconnectFromServer();

    bool isConnected() const { return m_connected; }

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &msg);

    // 业务信号：收到任务相关事件
    void taskCreated(const QJsonObject &taskObj);
    void taskUpdated(const QJsonObject &taskObj);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString &message);
    void onError(QAbstractSocket::SocketError err);

private:
    QWebSocket m_ws;
    bool m_connected = false;
};