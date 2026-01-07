#pragma once
#include <QWidget>
#include <QVariant>

class QLabel;
class QLineEdit;

class NodeInspectorTemplateWidget : public QWidget {
public:
    explicit NodeInspectorTemplateWidget(QWidget* parent = nullptr);

    void setValues(const QVariantMap& exec);
    QString templateIdValue() const;
    QString templateParamsValue() const;
    void setReadOnlyMode(bool ro);

private:
    QLabel* templateIdLabel_ = nullptr;
    QLabel* templateParamsLabel_ = nullptr;
    QLineEdit* templateIdEdit_ = nullptr;
    QLineEdit* templateParamsEdit_ = nullptr;
};
