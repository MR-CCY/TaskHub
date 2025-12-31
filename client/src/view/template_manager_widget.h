#pragma once
#include <QWidget>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QModelIndex>

class QPushButton;
class QTableView;
class QStandardItemModel;
class ApiClient;

// 模版管理页：左侧模板列表，右侧参数定义列表，支持运行选中模板
class TemplateManagerWidget : public QWidget {
    Q_OBJECT
public:
    explicit TemplateManagerWidget(QWidget* parent = nullptr);
    void setApiClient(ApiClient* api);

private slots:
    void onRefresh();
    void onTemplates(const QJsonArray& items);
    void onTemplateRowChanged(const QModelIndex& current, const QModelIndex& previous);
    void onRunTemplate();

private:
    void buildUi();
    void fillTemplateRow(const QJsonObject& obj);
    void showParamsForRow(int row);
    bool buildParamsPayload(QJsonObject& outParams);
    QString jsonValueToString(const QJsonValue& v) const;
    bool parseValueByType(const QString& type, const QString& text, QJsonValue& out, QString& err) const;

private:
    ApiClient* api_ = nullptr;
    QPushButton* refreshBtn_ = nullptr;
    QPushButton* runBtn_ = nullptr;
    QTableView* templateView_ = nullptr;
    QTableView* paramsView_ = nullptr;
    QStandardItemModel* templateModel_ = nullptr;
    QStandardItemModel* paramsModel_ = nullptr;
};
