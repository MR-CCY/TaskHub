#pragma once
#include <QWidget>
#include <QVariant>

class QLabel;
class QLineEdit;

class NodeInspectorLocalWidget : public QWidget {
    Q_OBJECT
public:
    explicit NodeInspectorLocalWidget(QWidget* parent = nullptr);

    void setValues(const QVariantMap& exec);
    QString handlerValue() const;
    QString aValue() const;
    QString bValue() const;
    void setReadOnlyMode(bool ro);

private:
    QLabel* handlerLabel_ = nullptr;
    QLineEdit* handlerEdit_ = nullptr;
    QLabel* aLabel_ = nullptr;
    QLineEdit* aEdit_ = nullptr;
    QLabel* bLabel_ = nullptr;
    QLineEdit* bEdit_ = nullptr;
};
