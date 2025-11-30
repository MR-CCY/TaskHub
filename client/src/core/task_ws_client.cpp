#include "task_ws_client.h"
#include <QJsonDocument>

TaskWsClient::TaskWsClient(QObject *parent)
    : QObject(parent)
{
    connect(&m_ws, &QWebSocket::connected,
        this, &TaskWsClient::onConnected);
connect(&m_ws, &QWebSocket::disconnected,
        this, &TaskWsClient::onDisconnected);
connect(&m_ws, &QWebSocket::textMessageReceived,
        this, &TaskWsClient::onTextMessageReceived);
connect(&m_ws,
        QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred),
        this, &TaskWsClient::onError);
}

void TaskWsClient::connectToServer(const QUrl &url)
{
    if (m_connected) {
        qDebug() << "[WS] already connected";
        return;
    }
    qDebug() << "[WS] connecting to" << url;
    m_ws.open(url);
}

void TaskWsClient::disconnectFromServer()
{
    if (!m_connected)
        return;
    m_ws.close();
}

void TaskWsClient::onConnected()
{
    m_connected = true;
    qDebug() << "[WS] connected";
    emit connected();
}


void TaskWsClient::onDisconnected()
{
    m_connected = false;
    qDebug() << "[WS] disconnected";
    emit disconnected();
}
void TaskWsClient::onTextMessageReceived(const QString &message)
{
    qDebug() << "[WS] message:" << message;

    QJsonParseError jsonErr;
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &jsonErr);
    if (jsonErr.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "[WS] invalid json:" << jsonErr.errorString();
        return;
    }

    const QJsonObject root = doc.object();
    const QString event    = root.value(QStringLiteral("event")).toString();
    const QJsonObject data = root.value(QStringLiteral("data")).toObject();

    if (event == QLatin1String("task_created")) {
        emit taskCreated(data);
    } else if (event == QLatin1String("task_updated")) {
        emit taskUpdated(data);
    } else {
        qDebug() << "[WS] unknown event:" << event;
    }
}
void TaskWsClient::onError(QAbstractSocket::SocketError message) 
{
    const QString msg = m_ws.errorString();
    qWarning() << "[WS] socket error:" << message << msg;
    emit errorOccurred(msg);
}
