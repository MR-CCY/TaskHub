#include "node_inspector_local_widget.h"

#include <QAbstractItemView>
#include <QFormLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>

namespace {
constexpr int kParamTypeRole = Qt::UserRole + 2;
constexpr int kParamRequiredRole = Qt::UserRole + 3;
constexpr int kParamDefaultRole = Qt::UserRole + 4;
}

NodeInspectorLocalWidget::NodeInspectorLocalWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(6);

    handlerLabel_ = new QLabel(tr("处理器 (Handler):"), this);
    handlerEdit_ = new QLineEdit(this);
    handlerEdit_->setPlaceholderText(tr("从模板读取 exec_command"));
    handlerEdit_->setReadOnly(true);

    form->addRow(handlerLabel_, handlerEdit_);
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

void NodeInspectorLocalWidget::setValues(const QVariantMap& props, const QVariantMap& exec)
{
    handlerEdit_->setText(props.value("exec_command").toString());
    paramsModel_->removeRows(0, paramsModel_->rowCount());

    const QJsonObject schemaObj = parseSchemaObject(props);
    const QJsonArray schemaParams = schemaObj.value("params").toArray();

    if (schemaParams.isEmpty()) {
        for (auto it = exec.begin(); it != exec.end(); ++it) {
            const QString name = it.key();
            auto* nameItem = new QStandardItem(name);
            auto* typeItem = new QStandardItem(QStringLiteral("string"));
            auto* reqItem = new QStandardItem(tr("否"));
            auto* defItem = new QStandardItem(variantValueToString(it.value()));

            nameItem->setEditable(false);
            typeItem->setEditable(false);
            reqItem->setEditable(false);
            defItem->setEditable(!readOnly_);
            defItem->setData(QStringLiteral("string"), kParamTypeRole);
            defItem->setData(false, kParamRequiredRole);
            defItem->setData(QJsonValue(), kParamDefaultRole);

            paramsModel_->appendRow({nameItem, typeItem, reqItem, defItem});
        }
        return;
    }

    for (const auto& v : schemaParams) {
        if (!v.isObject()) continue;
        const QJsonObject p = v.toObject();
        const QString name = p.value("name").toString();
        const QString type = p.value("type").toString();
        const bool required = p.value("required").toBool(false);
        const QJsonValue defVal = p.value("defaultValue");

        QString valueText;
        if (exec.contains(name)) {
            valueText = variantValueToString(exec.value(name));
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

QString NodeInspectorLocalWidget::handlerValue() const
{
    return handlerEdit_->text();
}

bool NodeInspectorLocalWidget::buildParamsPayload(QJsonObject& outParams)
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

void NodeInspectorLocalWidget::setReadOnlyMode(bool ro)
{
    readOnly_ = ro;
    if (paramsView_) {
        paramsView_->setEditTriggers(ro ? QAbstractItemView::NoEditTriggers
                                        : QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    }
}

QJsonObject NodeInspectorLocalWidget::parseSchemaObject(const QVariantMap& props) const
{
    QVariant schemaVar;
    if (props.contains("schema")) {
        schemaVar = props.value("schema");
    } else if (props.contains("schema_json")) {
        schemaVar = props.value("schema_json");
    }
    if (!schemaVar.isValid()) return QJsonObject();

    if (schemaVar.canConvert<QJsonObject>()) {
        return schemaVar.toJsonObject();
    }
    if (schemaVar.canConvert<QJsonArray>()) {
        QJsonObject obj;
        obj.insert("params", schemaVar.toJsonArray());
        return obj;
    }
    if (schemaVar.canConvert<QVariantMap>()) {
        return QJsonObject::fromVariantMap(schemaVar.toMap());
    }
    if (schemaVar.canConvert<QString>()) {
        const QString raw = schemaVar.toString();
        QJsonParseError pe;
        const auto doc = QJsonDocument::fromJson(raw.toUtf8(), &pe);
        if (pe.error == QJsonParseError::NoError) {
            if (doc.isObject()) return doc.object();
            if (doc.isArray()) {
                QJsonObject obj;
                obj.insert("params", doc.array());
                return obj;
            }
        }
    }
    return QJsonObject();
}

QString NodeInspectorLocalWidget::jsonValueToString(const QJsonValue& v) const
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

QString NodeInspectorLocalWidget::variantValueToString(const QVariant& v) const
{
    if (!v.isValid()) return QString();
    return jsonValueToString(QJsonValue::fromVariant(v));
}

bool NodeInspectorLocalWidget::parseValueByType(const QString& type, const QString& text, QJsonValue& out, QString& err) const
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
