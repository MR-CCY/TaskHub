#pragma once

#include <QDialog>
#include <QJsonObject>
#include <QHash>
#include <QTimer>

class ApiClient;
class DagRunViewerBench;
class InspectorPanel;
class ConsoleDock;

class DagRunMonitorDialog : public QDialog {
    Q_OBJECT
public:
    DagRunMonitorDialog(const QString& runId,
                        const QString& dagJson,
                        ApiClient* api,
                        QWidget* parent = nullptr);
    ~DagRunMonitorDialog() override;

private slots:
    void fetchTaskRuns();
    void fetchEvents();
    void onTimelineTick();

    // API Response Slots
    void onTaskRuns(const QJsonArray& items);
    void onRemoteTaskRuns(const QString& remotePath, const QJsonArray& items);
    void onDagEvents(const QJsonArray& items);
    void onRemoteDagEvents(const QString& remotePath, const QJsonArray& items);

private:
    void buildUi();
    void populateTaskRuns(const QJsonArray& items);
    void appendEvents(const QJsonArray& items);
    QColor statusColor(int status) const;
    QString statusText(int status) const;
    void updateNodeStatusFromEvent(const QJsonObject& ev);

    void fetchRemoteTaskRuns(const QString& remotePath);
    void fetchRemoteEvents(const QString& remotePath);
    void mergeRemoteTaskRuns(const QString& remotePrefix, const QJsonArray& items);
    void mergeRemoteEvents(const QString& remotePrefix, const QJsonArray& items);

private:
    QString runId_;
    QString dagJson_;
    ApiClient* api_ = nullptr;
    DagRunViewerBench* bench_ = nullptr;
    InspectorPanel* inspector_ = nullptr;
    ConsoleDock* consoleDock_ = nullptr;
    QTimer pollTimer_;
    qint64 lastEventTs_ = 0;
    int pollCounter_ = 0;
    
    QMap<QString, qint64> remoteLastEventTs_; 
    QMap<QString, QJsonObject> allTaskRuns_; 
};
