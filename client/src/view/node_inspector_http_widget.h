#pragma once
#include <QWidget>
#include <QVariant>

class QComboBox;
class QLabel;
class QLineEdit;
class QCheckBox;

class NodeInspectorHttpWidget : public QWidget {
public:
    explicit NodeInspectorHttpWidget(QWidget* parent = nullptr);

    void setValues(const QVariantMap& exec);
    QString urlValue() const;
    QString methodValue() const;
    QString headersValue() const;
    QString bodyValue() const;
    QString authUserValue() const;
    QString authPassValue() const;
    bool followRedirectsValue() const;

    void setReadOnlyMode(bool ro);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    QLabel* urlLabel_ = nullptr;
    QLabel* methodLabel_ = nullptr;
    QLabel* headersLabel_ = nullptr;
    QLabel* bodyLabel_ = nullptr;
    QLabel* authUserLabel_ = nullptr;
    QLabel* authPassLabel_ = nullptr;
    QLabel* followLabel_ = nullptr;

    QLineEdit* urlEdit_ = nullptr;
    QComboBox* methodCombo_ = nullptr;
    QLineEdit* headersEdit_ = nullptr;
    QLineEdit* bodyEdit_ = nullptr;
    QLineEdit* authUserEdit_ = nullptr;
    QLineEdit* authPassEdit_ = nullptr;
    QCheckBox* followCheck_ = nullptr;

    QString fullHeaders_;
    QString fullBody_;
};
