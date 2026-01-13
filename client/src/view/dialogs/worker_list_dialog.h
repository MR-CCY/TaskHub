#pragma once
#include <QDialog>
#include <QJsonArray>

class QTableWidget;
class QComboBox;

class WorkerListDialog : public QDialog {
    Q_OBJECT
public:
    explicit WorkerListDialog(QWidget* parent = nullptr);

private slots:
    void onQueueFilterChanged(const QString& queue);

private:
    void setupUi();
    void loadWorkers();
    void filterWorkers();

    QComboBox* queueFilterCombo_ = nullptr;
    QTableWidget* table_ = nullptr;
    QJsonArray allWorkers_;
};
