#include "ws_client.h"
#include <QWebSocket>
#include <QJsonDocument>
#include <QJsonParseError>

WsClient::WsClient(QObject* parent)
    : QObject(parent),
      ws_(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this)) {

    connect(ws_, &QWebSocket::connected, this, [this]() {
        authed_ = false;
        emit connected();

        // API.md: first message must include token
        QJsonObject j;
        j["token"] = token_;
        sendJson(j);
    });

    connect(ws_, &QWebSocket::textMessageReceived, this, [this](const QString& text) {
        QJsonParseError pe;
        const auto doc = QJsonDocument::fromJson(text.toUtf8(), &pe);
        if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
            emit error(QString("ws bad json: %1").arg(pe.errorString()));
            return;
        }

        const QJsonObject obj = doc.object();
        emit messageReceived(obj);

        // API.md: {"type":"authed"} => success
        const QString type = obj.value("type").toString();
        if (!authed_ && type == "authed") {
            authed_ = true;
            emit authed();
        }
    });

    connect(ws_, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emit error(ws_->errorString());
    });

    connect(ws_, &QWebSocket::disconnected, this, [this]() {
        authed_ = false;
        emit closed();
    });
}

WsClient::~WsClient()
{
    for(auto& s : subscribedLogs_){
        unsubscribeTaskLogs(s.first,s.second);
    }
    for(auto& s : subscribedEvents_){
        unsubscribeTaskEvents(s.first,s.second);
    }
}

void WsClient::setToken(const QString& token) {
    token_ = token;
}

void WsClient::connectTo(const QUrl& wsUrl) {
    if (token_.isEmpty()) {
        emit error("ws connect refused: token is empty");
        return;
    }
    ws_->open(wsUrl);
}

void WsClient::close() {
    ws_->close();
}

void WsClient::subscribeTaskLogs(const QString &taskId, const QString &runId)
{
    QJsonObject j;
    j["op"] = "subscribe";
    j["topic"] = "task_logs";
    j["task_id"] = taskId;
    if (!runId.isEmpty()) j["run_id"] = runId;
    j["token"] = token_;
    subscribedLogs_.insert({taskId, runId});
    // 注意：你的服务端要求首条消息带 token，后续消息不要求 token（你实现是 authed_ 后不再校验 token 字段）
    // 但带上 token 也没坏处；为了简单可以不带。
    sendJson(j);
}

void WsClient::unsubscribeTaskLogs(const QString &taskId, const QString &runId)
{
    QJsonObject j;
    j["op"] = "unsubscribe";
    j["topic"] = "task_logs";
    j["task_id"] = taskId;
    j["token"] = token_;
    if (!runId.isEmpty()) j["run_id"] = runId;
    sendJson(j);
}

void WsClient::subscribeTaskEvents(const QString &taskId, const QString &runId)
{
    QJsonObject j;
    j["op"] = "subscribe";
    j["topic"] = "task_events";
    j["task_id"] = taskId;
    if (!runId.isEmpty()) j["run_id"] = runId;
    j["token"] = token_;
    subscribedEvents_.insert({taskId, runId});
    sendJson(j);
}

void WsClient::unsubscribeTaskEvents(const QString &taskId, const QString &runId)
{
    QJsonObject j;
    j["op"] = "unsubscribe";
    j["topic"] = "task_events";
    j["task_id"] = taskId;
    j["token"] = token_;
    if (!runId.isEmpty()) j["run_id"] = runId;
    sendJson(j);
}

void WsClient::sendJson(const QJsonObject& obj) {
    const QByteArray bytes = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    ws_->sendTextMessage(QString::fromUtf8(bytes));
}
