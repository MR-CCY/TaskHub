#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QHeaderView>

class HeadersEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit HeadersEditorDialog(const QString& headersText, bool readOnly = false, QWidget* parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(tr(readOnly ? "查看请求头" : "编辑请求头"));
        resize(500, 400);

        auto* layout = new QVBoxLayout(this);

        table_ = new QTableWidget(0, 2, this);
        table_->setHorizontalHeaderLabels({tr("键 (Key)"), tr("值 (Value)")});
        table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table_->setSelectionBehavior(QAbstractItemView::SelectRows);
        if (readOnly) {
            table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        }

        // 解析初始显示
        QStringList lines = headersText.split("\n", Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            int colon = line.indexOf(':');
            if (colon > 0) {
                QString k = line.left(colon).trimmed();
                QString v = line.mid(colon + 1).trimmed();
                addRow(k, v);
            }
        }

        layout->addWidget(table_);

        auto* btnLayout = new QHBoxLayout();
        auto* addBtn = new QPushButton(tr("新增一行"), this);
        auto* delBtn = new QPushButton(tr("删除选中"), this);
        auto* okBtn = new QPushButton(tr("确定"), this);
        auto* cancelBtn = new QPushButton(tr(readOnly ? "关闭" : "取消"), this);

        btnLayout->addWidget(addBtn);
        btnLayout->addWidget(delBtn);
        btnLayout->addStretch();
        if (!readOnly) {
            btnLayout->addWidget(okBtn);
        }
        btnLayout->addWidget(cancelBtn);

        layout->addLayout(btnLayout);

        if (readOnly) {
            addBtn->setVisible(false);
            delBtn->setVisible(false);
        }

        connect(addBtn, &QPushButton::clicked, this, [this](){ addRow("", ""); });
        connect(delBtn, &QPushButton::clicked, this, [this](){
            auto items = table_->selectedItems();
            if (!items.empty()) {
                table_->removeRow(items.first()->row());
            }
        });
        connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    }

    QString getHeadersText() const {
        QStringList lines;
        for (int i = 0; i < table_->rowCount(); ++i) {
            auto* kItem = table_->item(i, 0);
            auto* vItem = table_->item(i, 1);
            if (kItem && !kItem->text().trimmed().isEmpty()) {
                lines << QString("%1: %2").arg(kItem->text().trimmed(), vItem ? vItem->text().trimmed() : "");
            }
        }
        return lines.join("\n");
    }

private:
    void addRow(const QString& k, const QString& v) {
        int row = table_->rowCount();
        table_->insertRow(row);
        table_->setItem(row, 0, new QTableWidgetItem(k));
        table_->setItem(row, 1, new QTableWidgetItem(v));
    }

    QTableWidget* table_;
};
