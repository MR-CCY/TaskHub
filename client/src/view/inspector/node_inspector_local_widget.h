#pragma once
#include <QWidget>
#include <QJsonObject>
#include <QVariant>

class QLabel;
class QLineEdit;
class QStandardItemModel;
class QTableView;

class NodeInspectorLocalWidget : public QWidget {
    Q_OBJECT
public:
    explicit NodeInspectorLocalWidget(QWidget* parent = nullptr);

    void setValues(const QVariantMap& props, const QVariantMap& exec);
    QString handlerValue() const;
    bool buildParamsPayload(QJsonObject& outParams);
    void setReadOnlyMode(bool ro);

private:
    QJsonObject parseSchemaObject(const QVariantMap& props) const;
    QString jsonValueToString(const QJsonValue& v) const;
    QString variantValueToString(const QVariant& v) const;
    bool parseValueByType(const QString& type, const QString& text, QJsonValue& out, QString& err) const;

    QLabel* handlerLabel_ = nullptr;
    QLineEdit* handlerEdit_ = nullptr;
    QTableView* paramsView_ = nullptr;
    QStandardItemModel* paramsModel_ = nullptr;
    bool readOnly_ = false;
};
