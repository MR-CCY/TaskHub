#pragma once
#include <QWidget>
#include <QVariant>

class QLabel;
class QLineEdit;

class NodeInspectorDagWidget : public QWidget {
public:
    explicit NodeInspectorDagWidget(QWidget* parent = nullptr);

    void setValues(const QVariantMap& exec);
    QString dagJsonValue() const;
    void setReadOnlyMode(bool ro);

private:
    QLabel* dagJsonLabel_ = nullptr;
    QLineEdit* dagJsonEdit_ = nullptr;
};
