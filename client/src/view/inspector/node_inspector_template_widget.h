#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QVariant>
#include <QWidget>

class ApiClient;
class QLabel;
class QLineEdit;
class QStandardItemModel;
class QTableView;

class NodeInspectorTemplateWidget : public QWidget {
    Q_OBJECT
public:
    explicit NodeInspectorTemplateWidget(QWidget* parent = nullptr);

    void setApiClient(ApiClient* api);
    void setValues(const QVariantMap& exec);
    QString templateIdValue() const;
    bool buildParamsPayload(QJsonObject& outParams);
    void setReadOnlyMode(bool ro);

private slots:
    void onTemplates(const QJsonArray& items);

private:
    void buildUi();
    void refreshParamsForTemplate(const QString& templateId, const QVariantMap& params);
    QVariantMap extractParamsMap(const QVariantMap& exec) const;
    QJsonObject findTemplateById(const QString& templateId) const;
    QJsonObject parseSchemaObject(const QJsonObject& tplObj) const;
    QString jsonValueToString(const QJsonValue& v) const;
    QString variantValueToString(const QVariant& v) const;
    bool parseValueByType(const QString& type, const QString& text, QJsonValue& out, QString& err) const;

private:
    ApiClient* api_ = nullptr;
    QJsonArray templates_;
    QString pendingTemplateId_;
    QVariantMap pendingParams_;
    bool awaitingTemplates_ = false;
    bool readOnly_ = false;

    QLabel* templateIdLabel_ = nullptr;
    QLineEdit* templateIdEdit_ = nullptr;
    QTableView* paramsView_ = nullptr;
    QStandardItemModel* paramsModel_ = nullptr;
};
