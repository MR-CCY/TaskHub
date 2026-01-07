#pragma once
#include <QWidget>
#include <QVariant>

class QComboBox;
class QLabel;
class QLineEdit;

class NodeInspectorHttpWidget : public QWidget {
public:
    explicit NodeInspectorHttpWidget(QWidget* parent = nullptr);

    void setValues(const QVariantMap& exec);
    QString methodValue() const;
    QString bodyValue() const;
    void setReadOnlyMode(bool ro);

private:
    QLabel* methodLabel_ = nullptr;
    QLabel* bodyLabel_ = nullptr;
    QComboBox* methodCombo_ = nullptr;
    QLineEdit* bodyEdit_ = nullptr;
};
