#pragma once

#include <QDialog>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFontDatabase>

class BodyEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit BodyEditorDialog(const QString& bodyText, bool readOnly = false, QWidget* parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(tr(readOnly ? "查看请求体" : "编辑请求体"));
        resize(600, 500);

        auto* layout = new QVBoxLayout(this);

        editor_ = new QPlainTextEdit(this);
        editor_->setPlainText(bodyText);
        editor_->setReadOnly(readOnly);
        
        // 使用等宽字体
        QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        editor_->setFont(monoFont);

        layout->addWidget(editor_);

        auto* btnLayout = new QHBoxLayout();
        auto* okBtn = new QPushButton(tr("确认"), this);
        auto* cancelBtn = new QPushButton(tr(readOnly ? "关闭" : "取消"), this);

        btnLayout->addStretch();
        if (!readOnly) {
            btnLayout->addWidget(okBtn);
        }
        btnLayout->addWidget(cancelBtn);

        layout->addLayout(btnLayout);

        connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    }

    QString getBodyText() const {
        return editor_->toPlainText();
    }

private:
    QPlainTextEdit* editor_;
};
