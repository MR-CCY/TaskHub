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
    void listTemplates();
    void runTemplateAsync(const QString& templateId, const QJsonObject& params);
    void getDagRuns(const QString& runId = QString(), const QString& name = QString(),
                    qint64 startTsMs = 0, qint64 endTsMs = 0, int limit = 100);
    void getTaskRuns(const QString& runId = QString(), const QString& name = QString(), int limit = 200);
    // Proxy for remote task runs
    void getRemoteTaskRuns(const QString& runId, const QString& remotePath, int limit = 200);

    void getDagEvents(const QString& runId = QString(), const QString& taskId = QString(),
                      const QString& type = QString(), const QString& event = QString(),
                      qint64 startTsMs = 0, qint64 endTsMs = 0, int limit = 200);
    // Proxy for remote dag events
    void getRemoteDagEvents(const QString& runId, const QString& remotePath, qint64 startTsMs = 0, int limit = 200);
    void listCronJobs();
    void deleteCronJob(const QString& id);
    void createCronJob(const QJsonObject& body);
    void getWorkers();

signals:
    void unauthorized();
    // login
    void loginOk(const QString& token, const QString& username);
    void loginFailed(int httpStatus, const QString& message);

    // health/info
    void healthOk(const QJsonObject& data);
    void infoOk(const QJsonObject& data);
    void templatesOk(const QJsonArray& items);
    void dagRunsOk(const QJsonArray& items);
    void taskRunsOk(const QJsonArray& items);
    void remoteTaskRunsOk(const QString& remotePath, const QJsonArray& items);
    
    void dagEventsOk(const QJsonArray& items);
    void remoteDagEventsOk(const QString& remotePath, const QJsonArray& items);
    void runDagAsyncOk(const QString& runId, const QJsonArray& taskIds);
    void runTemplateAsyncOk(const QString& runId, const QJsonArray& taskIds);
    void cronJobsOk(const QJsonArray& jobs);
    void cronJobDeleted(const QString& id);
    void cronJobCreated(const QString& id);
    void workersOk(const QJsonArray& workers);

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
    void deleteJson(const QString& apiName, const QString& path, const QString& idContext = QString());

    static bool parseJsonObject(const QByteArray& bytes, QJsonObject& out, QString& err);
    static ParsedEnvelope parseEnvelope(const QJsonObject& root);

private slots:
    void onRawJson(const QString& apiName, const QJsonObject& root);

private:
    QNetworkAccessManager* nam_ = nullptr;
    QString baseUrl_;
    QString token_;
    bool unauthorized_=  false;
};
