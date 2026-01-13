#pragma once
#include <QWidget>
#include <QJsonArray>

class QPushButton;
class QTableView;
class QStandardItemModel;
class ApiClient;

// Cron jobs query/delete widget
class CronJobsWidget : public QWidget {
    Q_OBJECT
public:
    explicit CronJobsWidget(QWidget* parent = nullptr);
    void setApiClient(ApiClient* api);

private slots:
    void onRefresh();
    void onDelete();
    void onCronJobs(const QJsonArray& items);
    void onCronChanged();

private:
    void buildUi();
    QString selectedId() const;
    void fillRow(int row, const QJsonObject& obj);

    ApiClient* api_ = nullptr;
    QPushButton* refreshBtn_ = nullptr;
    QPushButton* deleteBtn_ = nullptr;
    QTableView* table_ = nullptr;
    QStandardItemModel* model_ = nullptr;
};
