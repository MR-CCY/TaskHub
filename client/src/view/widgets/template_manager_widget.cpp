#include "template_manager_widget.h"
#include "utils/id_utils.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>

#include "net/api_client.h"

namespace {
constexpr int kTemplateDataRole = Qt::UserRole + 1;
constexpr int kParamTypeRole = Qt::UserRole + 2;
constexpr int kParamRequiredRole = Qt::UserRole + 3;
constexpr int kParamDefaultRole = Qt::UserRole + 4;
}

TemplateManagerWidget::TemplateManagerWidget(QWidget* parent)
    : QWidget(parent) {
    buildUi();
}

void TemplateManagerWidget::setApiClient(ApiClient* api) {
    if (api_ == api) return;
    if (api_) {
        disconnect(api_, nullptr, this, nullptr);
    }
    api_ = api;
    if (api_) {
        connect(api_, &ApiClient::templatesOk, this, &TemplateManagerWidget::onTemplates);
        onRefresh();
    }
}

void TemplateManagerWidget::buildUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    auto* controls = new QHBoxLayout();
    controls->setContentsMargins(0, 0, 0, 0);
    controls->setSpacing(8);
    runBtn_ = new QPushButton(tr("运行模版"), this);
    refreshBtn_ = new QPushButton(tr("刷新"), this);
    runBtn_->setEnabled(false);
    controls->addWidget(runBtn_);
    controls->addWidget(refreshBtn_);
    controls->addStretch(1);
    mainLayout->addLayout(controls);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    templateView_ = new QTableView(splitter);
    paramsView_ = new QTableView(splitter);
    splitter->addWidget(templateView_);
    splitter->addWidget(paramsView_);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);
    mainLayout->addWidget(splitter);

    templateModel_ = new QStandardItemModel(this);
    templateModel_->setHorizontalHeaderLabels({tr("template_id"), tr("名称"), tr("描述")});
    templateView_->setModel(templateModel_);
    templateView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    templateView_->setSelectionMode(QAbstractItemView::SingleSelection);
    templateView_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    templateView_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    templateView_->verticalHeader()->setVisible(false);

    paramsModel_ = new QStandardItemModel(this);
    paramsModel_->setHorizontalHeaderLabels({tr("名称"), tr("类型"), tr("必填"), tr("默认值")});
    paramsView_->setModel(paramsModel_);
    paramsView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    paramsView_->setSelectionMode(QAbstractItemView::SingleSelection);
    paramsView_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    paramsView_->verticalHeader()->setVisible(false);

    connect(refreshBtn_, &QPushButton::clicked, this, &TemplateManagerWidget::onRefresh);
    connect(runBtn_, &QPushButton::clicked, this, &TemplateManagerWidget::onRunTemplate);
    if (auto* sel = templateView_->selectionModel()) {
        connect(sel, &QItemSelectionModel::currentRowChanged,
                this, &TemplateManagerWidget::onTemplateRowChanged);
    }
}

void TemplateManagerWidget::onRefresh() {
    if (!api_) {
        QMessageBox::warning(this, tr("提示"), tr("ApiClient 未设置"));
        return;
    }
    api_->listTemplates();
}

void TemplateManagerWidget::onTemplates(const QJsonArray& items) {
    templateModel_->removeRows(0, templateModel_->rowCount());
    for (const auto& v : items) {
        if (!v.isObject()) continue;
        fillTemplateRow(v.toObject());
    }
    if (templateModel_->rowCount() > 0) {
        templateView_->selectRow(0);
        showParamsForRow(0);
        runBtn_->setEnabled(true);
    } else {
        paramsModel_->removeRows(0, paramsModel_->rowCount());
        runBtn_->setEnabled(false);
    }
}

void TemplateManagerWidget::fillTemplateRow(const QJsonObject& obj) {
    QJsonObject tplObj = obj;
    if (!tplObj.contains("schema_json") && tplObj.contains("schema")) {
        tplObj.insert("schema_json", tplObj.value("schema"));
    }
    const QString id = tplObj.value("template_id").toString();
    const QString name = tplObj.value("name").toString();
    const QString desc = tplObj.value("description").toString();

    auto* idItem = new QStandardItem(id);
    idItem->setEditable(false);
    idItem->setData(tplObj, kTemplateDataRole);
    auto* nameItem = new QStandardItem(name);
    nameItem->setEditable(false);
    auto* descItem = new QStandardItem(desc);
    descItem->setEditable(false);
    templateModel_->appendRow({idItem, nameItem, descItem});
}

void TemplateManagerWidget::onTemplateRowChanged(const QModelIndex& current, const QModelIndex&) {
    if (!current.isValid()) {
        paramsModel_->removeRows(0, paramsModel_->rowCount());
        runBtn_->setEnabled(false);
        return;
    }
    runBtn_->setEnabled(true);
    showParamsForRow(current.row());
}

void TemplateManagerWidget::showParamsForRow(int row) {
    paramsModel_->removeRows(0, paramsModel_->rowCount());
    if (row < 0 || row >= templateModel_->rowCount()) return;
    auto* item = templateModel_->item(row, 0);
    if (!item) return;
    const QJsonObject tplObj = item->data(kTemplateDataRole).toJsonObject();
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

    const QJsonArray params = schemaObj.value("params").toArray();
    for (const auto& v : params) {
        if (!v.isObject()) continue;
        const QJsonObject p = v.toObject();
        const QString name = p.value("name").toString();
        const QString type = p.value("type").toString();
        const bool required = p.value("required").toBool(false);
        const QJsonValue defVal = p.value("defaultValue");

        auto* nameItem = new QStandardItem(name);
        auto* typeItem = new QStandardItem(type);
        auto* reqItem  = new QStandardItem(required ? tr("是") : tr("否"));
        auto* defItem  = new QStandardItem(jsonValueToString(defVal));

        nameItem->setEditable(false);
        typeItem->setEditable(false);
        reqItem->setEditable(false);
        defItem->setEditable(true);
        defItem->setData(type, kParamTypeRole);
        defItem->setData(required, kParamRequiredRole);
        defItem->setData(defVal, kParamDefaultRole);

        paramsModel_->appendRow({nameItem, typeItem, reqItem, defItem});
    }
}

QString TemplateManagerWidget::jsonValueToString(const QJsonValue& v) const {
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

bool TemplateManagerWidget::parseValueByType(const QString& type, const QString& text, QJsonValue& out, QString& err) const {
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
        // fallback: treat as string (ParamType::Json 允许字符串)
        out = trimmed;
        return true;
    }
    // default: string
    out = text;
    return true;
}

bool TemplateManagerWidget::buildParamsPayload(QJsonObject& outParams) {
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
            continue; // optional and empty -> rely on server默认值
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

void TemplateManagerWidget::onRunTemplate() {
    if (!api_) {
        QMessageBox::warning(this, tr("提示"), tr("ApiClient 未设置"));
        return;
    }
    const QModelIndex current = templateView_->currentIndex();
    if (!current.isValid()) {
        QMessageBox::information(this, tr("提示"), tr("请先选择一个模版"));
        return;
    }
    auto* item = templateModel_->item(current.row(), 0);
    if (!item) return;
    const QString templateId = item->text().trimmed();
    QJsonObject params;
    if (!buildParamsPayload(params)) {
        return;
    }
    const QString runId = taskhub::utils::generateRunId();
    api_->runTemplateAsync(templateId, params, runId);
}
