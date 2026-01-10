#pragma once

#include <QDialog>
#include <QJsonObject>
#include <QMap>
#include <QTimer>
#include <QStringList>

class ApiClient;
class DagRunViewerBench;
class InspectorPanel;
class ConsoleDock;

class DagRunMonitorDialog : public QDialog {
    Q_OBJECT
public:
    DagRunMonitorDialog(const QString& runId,const QString& dagJson,ApiClient* api,QWidget* parent = nullptr);
    ~DagRunMonitorDialog() override;

private slots:
    void fetchTaskRuns();
    void onTimelineTick();

    // API Response Slots
    void onRemoteTaskRuns(const QString& remotePath, const QJsonArray& items);
    void onRemoteDagEvents(const QString& remotePath, const QJsonArray& items);

private:
    void buildUi();
    QColor statusColor(int status) const;
    QString statusText(int status) const;
    void updateNodeStatusFromEvent(const QString &event, const QString &fullNodeId, const int& st);

    void mergeRemoteTaskRuns(const QJsonArray& items);
    void mergeRemoteEvents(const QJsonArray& items);

private:
    QString runId_;
    QString dagJson_;
    ApiClient* api_ = nullptr;
    DagRunViewerBench* bench_ = nullptr;
    InspectorPanel* inspector_ = nullptr;
    ConsoleDock* consoleDock_ = nullptr;
    QTimer pollTimer_;
    int pollCounter_ = 0;
    
    QMap<QString, QJsonObject> allTaskRuns_;
    QHash<QString, qint64> nodeLastEvntTs_;
    QMap<QString, QString> dagNodeRunIds_;      // DAG节点ID -> 其运行ID的映射
};
