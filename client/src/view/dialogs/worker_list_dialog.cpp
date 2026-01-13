#include "worker_list_dialog.h"
#include "core/app_context.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QComboBox>
#include <QLabel>
#include <QJsonObject>
#include <QSet>
#include <QDebug>
#include <iostream>
WorkerListDialog::WorkerListDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Worker 列表"));
    resize(600, 400);
    setupUi();
    loadWorkers();
}

void WorkerListDialog::setupUi()
{
    auto* layout = new QVBoxLayout(this);

    auto* filterLayout = new QHBoxLayout();
    filterLayout->addWidget(new QLabel(tr("队列:"), this));
    queueFilterCombo_ = new QComboBox(this);
    filterLayout->addWidget(queueFilterCombo_);
    filterLayout->addStretch();
    layout->addLayout(filterLayout);

    table_ = new QTableWidget(this);
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels({tr("ID"), tr("Host"), tr("Port"), tr("Status"), tr("Queues")});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(table_);

    connect(queueFilterCombo_, &QComboBox::currentTextChanged, this, &WorkerListDialog::onQueueFilterChanged);
}

void WorkerListDialog::loadWorkers()
{
    allWorkers_ = AppContext::instance().workers();
    qDebug() << allWorkers_;
    std::cout << "TEST OUTPUT" << std::endl;
    // 填充过滤器
    QSet<QString> queues;
    queues.insert(tr("全部"));
    for (const auto& v : allWorkers_) {
        QJsonObject w = v.toObject();
        QJsonArray qs = w.value("queues").toArray();
        for (const auto& qv : qs) {
            queues.insert(qv.toString());
        }
    }
    QStringList qList = queues.values();
    qList.sort();
    queueFilterCombo_->addItems(qList);
    queueFilterCombo_->setCurrentText(tr("全部"));

    filterWorkers();
}

void WorkerListDialog::onQueueFilterChanged(const QString&)
{
    filterWorkers();
}

void WorkerListDialog::filterWorkers()
{
    table_->setRowCount(0);
    QString filter = queueFilterCombo_->currentText();
    bool showAll = (filter == tr("全部"));

    for (const auto& v : allWorkers_) {
        QJsonObject w = v.toObject();
        QJsonArray qs = w.value("queues").toArray();
        bool match = showAll;
        QStringList qStrs;
        for (const auto& qv : qs) {
            QString s = qv.toString();
            qStrs << s;
            if (!showAll && s == filter) match = true;
        }
       
        if (match) {
            int row = table_->rowCount();
            table_->insertRow(row);
            table_->setItem(row, 0, new QTableWidgetItem(w.value("id").toString()));
            table_->setItem(row, 1, new QTableWidgetItem(w.value("host").toString()));
            table_->setItem(row, 2, new QTableWidgetItem(QString::number(w.value("port").toInt())));
            table_->setItem(row, 3, new QTableWidgetItem(w.value("status").toString()));
            table_->setItem(row, 4, new QTableWidgetItem(qStrs.join(", ")));
        }
    }
}
