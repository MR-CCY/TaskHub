#pragma once
#include <QWidget>
#include <QDateTime>

class QLineEdit;
class QComboBox;
class QDateTimeEdit;
class QPushButton;
class QTableView;
class QStandardItemModel;
class ApiClient;
class DagRunViewerBench;

// Simple DAG runs query widget.
class DagRunsWidget : public QWidget {
    Q_OBJECT
public:
    explicit DagRunsWidget(QWidget* parent = nullptr);
    void setApiClient(ApiClient* api);

private slots:
    void onSearch();
    void onDagRuns(const QJsonArray& items);
    void onRowDoubleClicked(const QModelIndex& index);

private:
    void buildUi();
    void fillRow(int row, const QJsonObject& obj);
    QString formatTs(qint64 ms) const;

private:
    ApiClient* api_ = nullptr;
    QLineEdit* runIdEdit_ = nullptr;
    QLineEdit* nameEdit_ = nullptr;
    QComboBox* sourceCombo_ = nullptr;
    QDateTimeEdit* startEdit_ = nullptr;
    QDateTimeEdit* endEdit_ = nullptr;
    QPushButton* searchBtn_ = nullptr;
    QTableView* table_ = nullptr;
    QStandardItemModel* model_ = nullptr;
};
