#include "node_inspector_template_widget.h"

#include <QAbstractItemView>
#include <QFormLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>

#include "net/api_client.h"

namespace {
constexpr int kParamTypeRole = Qt::UserRole + 2;
constexpr int kParamRequiredRole = Qt::UserRole + 3;
constexpr int kParamDefaultRole = Qt::UserRole + 4;
}

NodeInspectorTemplateWidget::NodeInspectorTemplateWidget(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void NodeInspectorTemplateWidget::setApiClient(ApiClient* api)
{
    if (api_ == api) return;
    if (api_) {
        disconnect(api_, nullptr, this, nullptr);
    }
    api_ = api;
    if (api_) {
        connect(api_, &ApiClient::templatesOk, this, &NodeInspectorTemplateWidget::onTemplates);
        api_->listTemplates();
    }
}

void NodeInspectorTemplateWidget::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(6);
    templateIdLabel_ = new QLabel(tr("template_id"), this);
    templateIdEdit_ = new QLineEdit(this);
    form->addRow(templateIdLabel_, templateIdEdit_);
    layout->addLayout(form);

    paramsView_ = new QTableView(this);
    paramsModel_ = new QStandardItemModel(this);
    paramsModel_->setHorizontalHeaderLabels({tr("名称"), tr("类型"), tr("必填"), tr("默认值")});
    paramsView_->setModel(paramsModel_);
    paramsView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    paramsView_->setSelectionMode(QAbstractItemView::SingleSelection);
    paramsView_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    paramsView_->verticalHeader()->setVisible(false);
    layout->addWidget(paramsView_);
}

void NodeInspectorTemplateWidget::setValues(const QVariantMap& exec)
{
    const QString templateId = exec.value("template_id").toString().trimmed();
    templateIdEdit_->setText(templateId);
    pendingTemplateId_ = templateId;
    pendingParams_ = extractParamsMap(exec);
    if (templateId.isEmpty()) {
        paramsModel_->removeRows(0, paramsModel_->rowCount());
        return;
    }

    const QJsonObject tplObj = findTemplateById(templateId);
    if (!tplObj.isEmpty()) {
        refreshParamsForTemplate(templateId, pendingParams_);
        return;
    }

    if (api_ && !awaitingTemplates_) {
        awaitingTemplates_ = true;
        api_->listTemplates();
    } else {
        paramsModel_->removeRows(0, paramsModel_->rowCount());
    }
}

QString NodeInspectorTemplateWidget::templateIdValue() const { return templateIdEdit_->text(); }

bool NodeInspectorTemplateWidget::buildParamsPayload(QJsonObject& outParams)
{
    outParams = QJsonObject();
    for (int row = 0; row < paramsModel_->rowCount(); ++row) {
        auto* nameItem = paramsModel_->item(row, 0);
        auto* defItem = paramsModel_->item(row, 3);
        if (!nameItem || !defItem) continue;
        const QString name = nameItem->text().trimmed();
        const QString type = defItem->data(kParamTypeRole).toString();
        const bool required = defItem->data(kParamRequiredRole).toBool();
        const QString rawText = defItem->text();
        const QString trimmed = rawText.trimmed();
        if (trimmed.isEmpty()) {
            if (required) {
                QMessageBox::warning(this, tr("参数缺失"),
                                     tr("必填参数 %1 不能为空").arg(name));
                return false;
            }
            continue;
        }
        QJsonValue val;
        QString err;
        if (!parseValueByType(type, rawText, val, err)) {
            QMessageBox::warning(this, tr("格式错误"),
                                 tr("%1: %2").arg(name, err));
            return false;
        }
        outParams.insert(name, val);
    }
    return true;
}

void NodeInspectorTemplateWidget::setReadOnlyMode(bool ro)
{
    readOnly_ = ro;
    if (templateIdEdit_) templateIdEdit_->setReadOnly(ro);
    if (paramsView_) {
        paramsView_->setEditTriggers(ro ? QAbstractItemView::NoEditTriggers
                                        : QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    }
}

void NodeInspectorTemplateWidget::onTemplates(const QJsonArray& items)
{
    templates_ = items;
    if (!awaitingTemplates_) return;
    awaitingTemplates_ = false;
    if (!pendingTemplateId_.isEmpty()) {
        refreshParamsForTemplate(pendingTemplateId_, pendingParams_);
    }
}

void NodeInspectorTemplateWidget::refreshParamsForTemplate(const QString& templateId, const QVariantMap& params)
{
    paramsModel_->removeRows(0, paramsModel_->rowCount());
    if (templateId.isEmpty()) return;
    const QJsonObject tplObj = findTemplateById(templateId);
    if (tplObj.isEmpty()) return;
    const QJsonObject schemaObj = parseSchemaObject(tplObj);
    const QJsonArray schemaParams = schemaObj.value("params").toArray();

    for (const auto& v : schemaParams) {
        if (!v.isObject()) continue;
        const QJsonObject p = v.toObject();
        const QString name = p.value("name").toString();
        const QString type = p.value("type").toString();
        const bool required = p.value("required").toBool(false);
        const QJsonValue defVal = p.value("defaultValue");

        QString valueText;
        if (params.contains(name)) {
            valueText = variantValueToString(params.value(name));
        } else {
            valueText = jsonValueToString(defVal);
        }

        auto* nameItem = new QStandardItem(name);
        auto* typeItem = new QStandardItem(type);
        auto* reqItem = new QStandardItem(required ? tr("是") : tr("否"));
        auto* defItem = new QStandardItem(valueText);

        nameItem->setEditable(false);
        typeItem->setEditable(false);
        reqItem->setEditable(false);
        defItem->setEditable(!readOnly_);
        defItem->setData(type, kParamTypeRole);
        defItem->setData(required, kParamRequiredRole);
        defItem->setData(defVal, kParamDefaultRole);

        paramsModel_->appendRow({nameItem, typeItem, reqItem, defItem});
    }
}

QVariantMap NodeInspectorTemplateWidget::extractParamsMap(const QVariantMap& exec) const
{
    if (exec.contains("params")) {
        const QVariant params = exec.value("params");
        if (params.canConvert<QVariantMap>()) {
            return params.toMap();
        }
        if (params.canConvert<QJsonObject>()) {
            return params.toJsonObject().toVariantMap();
        }
    }
    const QString jsonText = exec.value("template_params_json").toString().trimmed();
    if (jsonText.isEmpty()) return {};
    QJsonParseError pe;
    const auto doc = QJsonDocument::fromJson(jsonText.toUtf8(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) return {};
    return doc.object().toVariantMap();
}

QJsonObject NodeInspectorTemplateWidget::findTemplateById(const QString& templateId) const
{
    for (const auto& v : templates_) {
        if (!v.isObject()) continue;
        const QJsonObject obj = v.toObject();
        if (obj.value("template_id").toString() == templateId) {
            return obj;
        }
    }
    return {};
}

QJsonObject NodeInspectorTemplateWidget::parseSchemaObject(const QJsonObject& tplObj) const
{
    QJsonValue schemaVal = tplObj.value("schema");
    if (schemaVal.isUndefined() && tplObj.contains("schema_json")) {
        schemaVal = tplObj.value("schema_json");
    }

    QJsonObject schemaObj;
    if (schemaVal.isObject()) {
        schemaObj = schemaVal.toObject();
    } else if (schemaVal.isString()) {
        QJsonParseError pe;
        const auto doc = QJsonDocument::fromJson(schemaVal.toString().toUtf8(), &pe);
        if (pe.error == QJsonParseError::NoError && doc.isObject()) {
            schemaObj = doc.object();
        } else if (pe.error == QJsonParseError::NoError && doc.isArray()) {
            schemaObj.insert("params", doc.array());
        }
    }
    return schemaObj;
}

QString NodeInspectorTemplateWidget::jsonValueToString(const QJsonValue& v) const
{
    if (v.isNull() || v.isUndefined()) return QString();
    if (v.isString()) return v.toString();
    if (v.isBool()) return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (v.isDouble()) {
        const QVariant num = v.toVariant();
        return num.toString();
    }
    if (v.isArray()) {
        return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
    }
    if (v.isObject()) {
        return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    }
    return v.toVariant().toString();
}

QString NodeInspectorTemplateWidget::variantValueToString(const QVariant& v) const
{
    if (!v.isValid()) return QString();
    return jsonValueToString(QJsonValue::fromVariant(v));
}

bool NodeInspectorTemplateWidget::parseValueByType(const QString& type, const QString& text, QJsonValue& out, QString& err) const
{
    const QString t = type.trimmed().toLower();
    const QString trimmed = text.trimmed();
    if (t == "int" || t == "integer") {
        bool ok = false;
        const qlonglong v = trimmed.toLongLong(&ok);
        if (!ok) {
            err = tr("请输入整数");
            return false;
        }
        out = v;
        return true;
    }
    if (t == "bool" || t == "boolean") {
        const QString s = trimmed.toLower();
        if (s == "true" || s == "1" || s == "yes" || s == "y" || s == "on") {
            out = true;
            return true;
        }
        if (s == "false" || s == "0" || s == "no" || s == "n" || s == "off") {
            out = false;
            return true;
        }
        err = tr("布尔值仅支持 true/false/1/0");
        return false;
    }
    if (t == "json") {
        if (trimmed.compare("null", Qt::CaseInsensitive) == 0) {
            out = QJsonValue();
            return true;
        }
        if (trimmed.compare("true", Qt::CaseInsensitive) == 0) {
            out = true;
            return true;
        }
        if (trimmed.compare("false", Qt::CaseInsensitive) == 0) {
            out = false;
            return true;
        }
        bool okInt = false;
        const qlonglong iv = trimmed.toLongLong(&okInt);
        if (okInt) {
            out = iv;
            return true;
        }
        bool okDouble = false;
        const double dv = trimmed.toDouble(&okDouble);
        if (okDouble) {
            out = dv;
            return true;
        }
        QJsonParseError pe;
        const auto doc = QJsonDocument::fromJson(trimmed.toUtf8(), &pe);
        if (pe.error == QJsonParseError::NoError) {
            if (doc.isObject()) {
                out = doc.object();
                return true;
            }
            if (doc.isArray()) {
                out = doc.array();
                return true;
            }
        }
        out = trimmed;
        return true;
    }
    out = text;
    return true;
}
