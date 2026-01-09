#include "api_client.h"
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonValue>
#include <QUrlQuery>

namespace {
// If your server expects a different header, change only this constant.
constexpr const char* kAuthHeaderName = "Authorization";
}

ApiClient::ApiClient(QObject* parent)
    : QObject(parent),
      nam_(new QNetworkAccessManager(this)) {
          // Internal dispatch: listen to rawJson and route to specific signals
          connect(this, &ApiClient::rawJson, this, &ApiClient::onRawJson);
      }

void ApiClient::setBaseUrl(const QString& baseUrl) {
    baseUrl_ = baseUrl.trimmed();
    // remove trailing slash for safety
    while (baseUrl_.endsWith('/')) baseUrl_.chop(1);
}

void ApiClient::setToken(const QString& token) {
    token_ = token;
}

QNetworkRequest ApiClient::makeRequest(const QString& path) const {
    QUrl url(baseUrl_ + path);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=utf-8");

    if (!token_.isEmpty()) {
        // default: Authorization: Bearer <token>
        const QByteArray v = "Bearer " + token_.toUtf8();
        req.setRawHeader(kAuthHeaderName, v);
    }
    return req;
}

void ApiClient::login(const QString& username, const QString& password) {
    QJsonObject body;
    body["username"] = username;
    body["password"] = password;
    postJson("login", "/api/login", body);
}

void ApiClient::getHealth() {
    getJson("health", "/api/health");
}

void ApiClient::getInfo() {
    getJson("info", "/api/info");
}

void ApiClient::runDagAsync(const QJsonObject& body) {
    postJson("runDagAsync", "/api/dag/run_async", body);
}

void ApiClient::listTemplates() {
    getJson("templates", "/templates");
}

void ApiClient::runTemplateAsync(const QString& templateId, const QJsonObject& params) {
    QJsonObject body;
    body["template_id"] = templateId;
    body["params"] = params;
    postJson("runTemplateAsync", "/template/run_async", body);
}

void ApiClient::getDagRuns(const QString& runId, const QString& name, qint64 startTsMs, qint64 endTsMs, int limit) {
    QUrlQuery q;
    if (!runId.isEmpty()) q.addQueryItem("run_id", runId);
    if (!name.isEmpty()) q.addQueryItem("name", name);
    if (startTsMs > 0) q.addQueryItem("start_ts_ms", QString::number(startTsMs));
    if (endTsMs > 0) q.addQueryItem("end_ts_ms", QString::number(endTsMs));
    if (limit > 0) q.addQueryItem("limit", QString::number(limit));
    const QString path = "/api/dag/runs" + (q.isEmpty() ? QString() : QString("?" + q.toString(QUrl::FullyEncoded)));
    getJson("dagRuns", path);
}

void ApiClient::getTaskRuns(const QString& runId, const QString& name, int limit) {
    QUrlQuery q;
    if (!runId.isEmpty()) q.addQueryItem("run_id", runId);
    if (!name.isEmpty()) q.addQueryItem("name", name);
    if (limit > 0) q.addQueryItem("limit", QString::number(limit));
    const QString path = "/api/dag/task_runs" + (q.isEmpty() ? QString() : QString("?" + q.toString(QUrl::FullyEncoded)));
    getJson("taskRuns", path);
}

void ApiClient::getRemoteTaskRuns(const QString& runId, const QString& remotePath, int limit) {
    QUrlQuery q;
    if (!runId.isEmpty()) q.addQueryItem("run_id", runId);
    if (!remotePath.isEmpty()) q.addQueryItem("remote_path", remotePath);
    if (limit > 0) q.addQueryItem("limit", QString::number(limit));
    const QString path = "/api/workers/proxy/dag/task_runs" + (q.isEmpty() ? QString() : QString("?" + q.toString(QUrl::FullyEncoded)));
    // Encode remotePath in apiName to retrieve it in callback
    getJson("remoteTaskRuns:" + remotePath, path);
}

void ApiClient::getDagEvents(const QString& runId, const QString& taskId,
                             const QString& type, const QString& event,
                             qint64 startTsMs, qint64 endTsMs, int limit) {
    QUrlQuery q;
    if (!runId.isEmpty()) q.addQueryItem("run_id", runId);
    if (!taskId.isEmpty()) q.addQueryItem("task_id", taskId);
    if (!type.isEmpty()) q.addQueryItem("type", type);
    if (!event.isEmpty()) q.addQueryItem("event", event);
    if (startTsMs > 0) q.addQueryItem("start_ts_ms", QString::number(startTsMs));
    if (endTsMs > 0) q.addQueryItem("end_ts_ms", QString::number(endTsMs));
    if (limit > 0) q.addQueryItem("limit", QString::number(limit));
    const QString path = "/api/dag/events" + (q.isEmpty() ? QString() : QString("?" + q.toString(QUrl::FullyEncoded)));
    getJson("dagEvents", path);
}

void ApiClient::getRemoteDagEvents(const QString& runId, const QString& remotePath, qint64 startTsMs, int limit) {
    QUrlQuery q;
    if (!runId.isEmpty()) q.addQueryItem("run_id", runId);
    if (!remotePath.isEmpty()) q.addQueryItem("remote_path", remotePath);
    if (startTsMs > 0) q.addQueryItem("start_ts_ms", QString::number(startTsMs));
    if (limit > 0) q.addQueryItem("limit", QString::number(limit));
    // Note: server endpoint is /api/workers/proxy/dag/events
    const QString path = "/api/workers/proxy/dag/events" + (q.isEmpty() ? QString() : QString("?" + q.toString(QUrl::FullyEncoded)));
    getJson("remoteDagEvents:" + remotePath, path);
}

void ApiClient::listCronJobs() {
    getJson("cronJobs", "/api/cron/jobs");
}

void ApiClient::deleteCronJob(const QString& id) {
    if (id.isEmpty()) return;
    deleteJson("cronDelete:" + id, "/api/cron/jobs/" + id);
}

void ApiClient::createCronJob(const QJsonObject& body) {
    postJson("cronCreate", "/api/cron/jobs", body);
}

void ApiClient::getWorkers() {
    getJson("workers", "/api/workers");
}

void ApiClient::getJson(const QString& apiName, const QString& path) {
    if (baseUrl_.isEmpty()) {
        emit requestFailed(apiName, 0, "baseUrl is empty");
        return;
    }

    QNetworkRequest req = makeRequest(path);
    QNetworkReply* reply = nam_->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, apiName]() {
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray bytes = reply->readAll();
        reply->deleteLater();
        if (httpStatus == 401) {
            emit unauthorized();
            return;
        }

        QJsonObject root;
        QString err;
        if (!parseJsonObject(bytes, root, err)) {
            emit requestFailed(apiName, httpStatus, "bad json: " + err);
            return;
        }

        emit rawJson(apiName, root);
    });

    connect(reply, &QNetworkReply::errorOccurred, this, [this, apiName](QNetworkReply::NetworkError) {
        // finished() will still fire; keep signal noise minimal
        // You can emit here if you want immediate error feedback.
        Q_UNUSED(apiName);
    });
}

void ApiClient::postJson(const QString& apiName, const QString& path, const QJsonObject& body) {
    if (baseUrl_.isEmpty()) {
        emit requestFailed(apiName, 0, "baseUrl is empty");
        return;
    }

    QNetworkRequest req = makeRequest(path);
    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

    QNetworkReply* reply = nam_->post(req, payload);

    connect(reply, &QNetworkReply::finished, this, [this, reply, apiName]() {
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray bytes = reply->readAll();
        reply->deleteLater();
        if (httpStatus == 401) {
            emit unauthorized();
            return;
        }
        QJsonObject root;
        QString err;
        if (!parseJsonObject(bytes, root, err)) {
            emit requestFailed(apiName, httpStatus, "bad json: " + err);
            return;
        }

        emit rawJson(apiName, root);
    });
}

void ApiClient::deleteJson(const QString& apiName, const QString& path, const QString& idContext) {
    if (baseUrl_.isEmpty()) {
        emit requestFailed(apiName, 0, "baseUrl is empty");
        return;
    }
    QNetworkRequest req = makeRequest(path);
    QNetworkReply* reply = nam_->deleteResource(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, apiName, idContext]() {
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray bytes = reply->readAll();
        reply->deleteLater();
        if (httpStatus == 401) {
            emit unauthorized();
            return;
        }
        QJsonObject root;
        QString err;
        if (!parseJsonObject(bytes, root, err)) {
            emit requestFailed(apiName, httpStatus, "bad json: " + err);
            return;
        }
        emit rawJson(apiName, root);
    });
}

bool ApiClient::parseJsonObject(const QByteArray& bytes, QJsonObject& out, QString& err) {
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &pe);
    if (pe.error != QJsonParseError::NoError) {
        err = pe.errorString();
        return false;
    }
    if (!doc.isObject()) {
        err = "root is not object";
        return false;
    }
    out = doc.object();
    return true;
}

ApiClient::ParsedEnvelope ApiClient::parseEnvelope(const QJsonObject& root) {
    ParsedEnvelope env;
    env.root = root;

    // Style A: {code,message,data}
    if (root.contains("code")) {
        const int code = root.value("code").toInt(-1);
        const QString msg = root.value("message").toString();
        env.message = msg.isEmpty() ? QString("error code=%1").arg(code) : msg;
        env.data = root.value("data");
        env.ok = (code == 0);
        return env;
    }

    // Style B: {ok:true/false, ...}
    if (root.contains("ok")) {
        const bool ok = root.value("ok").toBool(false);
        env.ok = ok;
        // common fields: message/error
        const QString msg = root.value("message").toString();
        const QString err = root.value("error").toString();
        env.message = !err.isEmpty() ? err : (!msg.isEmpty() ? msg : (ok ? "ok" : "request failed"));
        // In ok-style, sometimes payload is in "data"
        env.data = root.value("data");
        return env;
    }

    // Unknown format: treat as error
    env.ok = false;
    env.message = "unknown response format (missing code/ok)";
    return env;
}

void ApiClient::onRawJson(const QString& apiName, const QJsonObject& root) {
    const ParsedEnvelope env = parseEnvelope(root);
    if (!env.ok) {
        // login has special signal
        if (apiName == "login") emit loginFailed(0, env.message); // status 0 or generic?
        else emit requestFailed(apiName, 0, env.message); 
        // Note: httpStatus is lost here if we don't pass it.
        // Usually requestFailed carries httpStatus. 
        // rawJson signal signature is (apiName, root). 
        // If we want httpStatus, we need to change rawJson signature or embed it in root (hacky).
        // Or we assume logic error has 0 status. 
        // Most logic errors (env.ok == false) are application level errors (code != 0), even if HTTP 200.
        // So status 0 or 200 is acceptable approximation, or we extend rawJson.
        // Given constraint "internal signal", we can change rawJson signature easily!
        return;
    }

    if (apiName == "health") {
        if (env.data.isObject()) emit healthOk(env.data.toObject());
        else emit requestFailed(apiName, 0, "health data is not an object");
    } else if (apiName == "info") {
        if (env.data.isObject()) emit infoOk(env.data.toObject());
        else emit requestFailed(apiName, 0, "info data is not an object");
    } else if (apiName == "templates") {
        QJsonArray items;
        if (env.data.isArray()) {
            items = env.data.toArray();
        } else if (env.data.isObject()) {
            items = env.data.toObject().value("data").toArray();
        }
        emit templatesOk(items);
    } else if (apiName == "dagRuns") {
        if (env.data.isObject()) {
            emit dagRunsOk(env.data.toObject().value("items").toArray());
        } else {
            emit dagRunsOk(QJsonArray());
        }
    } else if (apiName == "taskRuns") {
        if (env.data.isObject()) {
            emit taskRunsOk(env.data.toObject().value("items").toArray());
        } else {
            emit taskRunsOk(QJsonArray());
        }
    } else if (apiName == "dagEvents") {
        if (env.data.isObject()) {
            emit dagEventsOk(env.data.toObject().value("items").toArray());
        } else {
            emit dagEventsOk(QJsonArray());
        }
    } else if (apiName.startsWith("remoteTaskRuns:")) {
        QString rPath = apiName.mid(15); 
        if (env.data.isObject()) {
            emit remoteTaskRunsOk(rPath, env.data.toObject().value("items").toArray());
        } else {
            emit remoteTaskRunsOk(rPath, QJsonArray());
        }
    } else if (apiName.startsWith("remoteDagEvents:")) {
        QString rPath = apiName.mid(16);
        if (env.data.isObject()) {
            emit remoteDagEventsOk(rPath, env.data.toObject().value("items").toArray());
        } else {
            emit remoteDagEventsOk(rPath, QJsonArray());
        }
    } else if (apiName == "cronJobs") {
        QJsonArray jobs;
        if (env.data.isObject()) {
            jobs = env.data.toObject().value("jobs").toArray();
        } else if (env.data.isArray()) {
            jobs = env.data.toArray();
        }
        emit cronJobsOk(jobs);
    } else if (apiName == "workers") {
        if (!env.data.isObject()) {
            emit requestFailed(apiName, 0, "workers data is not an object");
            return;
        }
        const QJsonArray workers = env.data.toObject().value("workers").toArray();
        emit workersOk(workers);
    } else if (apiName == "login") {
        // ... login logic ...
        if (!env.data.isObject()) {
             emit loginFailed(0, "login data is not an object");
             return;
        }
        const QJsonObject dataObj = env.data.toObject();
        const QString token = dataObj.value("token").toString();
        const QString user  = dataObj.value("username").toString();
        if (token.isEmpty()) {
             emit loginFailed(0, "missing token in login response");
             return;
        }
        emit loginOk(token, user);
    } else if (apiName == "runDagAsync") {
        if (env.data.isObject()) {
            const auto obj = env.data.toObject();
            const QString runId = obj.value("run_id").toString();
            const QJsonArray taskIds = obj.value("task_ids").toArray();
            emit runDagAsyncOk(runId, taskIds);
        } else {
            emit runDagAsyncOk(QString(), QJsonArray());
        }
    } else if (apiName == "runTemplateAsync") {
        if (env.data.isObject()) {
            const auto obj = env.data.toObject();
            const QString runId = obj.value("run_id").toString();
            const QJsonArray taskIds = obj.value("task_ids").toArray();
            emit runTemplateAsyncOk(runId, taskIds);
        } else {
            emit runTemplateAsyncOk(QString(), QJsonArray());
        }
    } else if (apiName == "cronCreate") {
        QString jobId;
        if (env.data.isObject()) {
            jobId = env.data.toObject().value("job_id").toString();
        }
        emit cronJobCreated(jobId);
    } else if (apiName.startsWith("cronDelete:")) {
        QString id = apiName.mid(11);
        emit cronJobDeleted(id);
    }
}
