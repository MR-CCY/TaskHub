#include "node_palette_widget.h"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStyle>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "net/api_client.h"

namespace {
const struct ItemDef { QString text; NodeType type; QString icon; } kTypeDefs[] = {
    { QObject::tr("Shell 节点"),  NodeType::Shell,  QStringLiteral(":/icons/image/shell.png")  },
    { QObject::tr("HTTP 节点"),   NodeType::Http,   QStringLiteral(":/icons/image/http.png")   },
    { QObject::tr("Local 节点"),  NodeType::Local,  QStringLiteral(":/icons/image/local.png")  },
    { QObject::tr("Remote 节点"), NodeType::Remote, QStringLiteral(":/icons/image/remote.png") },
    { QObject::tr("DAG 节点"),    NodeType::Dag,    QStringLiteral(":/icons/image/dag.png") },
    { QObject::tr("模版 节点"),   NodeType::Template, QStringLiteral(":/icons/image/template.png") },
};
}

NodePaletteWidget::NodePaletteWidget(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void NodePaletteWidget::setApiClient(ApiClient* api)
{
    if (api_ == api) return;
    if (api_) {
        disconnect(api_, nullptr, this, nullptr);
    }
    api_ = api;
    if (api_) {
        connect(api_, &ApiClient::templatesOk, this, &NodePaletteWidget::onTemplates);
        connect(api_, &ApiClient::taskTemplatesOk, this, &NodePaletteWidget::onTaskTemplates);
        api_->listTemplates();
        api_->listTaskTemplates();
    }
}

void NodePaletteWidget::buildUi()
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(6, 6, 6, 6);
    vbox->setSpacing(8);

    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(6);
    filterEdit_ = new QLineEdit(this);
    filterEdit_->setPlaceholderText(tr("筛选节点"));
    addButton_ = new QPushButton(tr("添加"), this);
    topRow->addWidget(filterEdit_);
    topRow->addWidget(addButton_);
    vbox->addLayout(topRow);

    auto* listsRow = new QHBoxLayout();
    listsRow->setSpacing(6);
    typeTree_ = new QTreeWidget(this);
    typeTree_->setHeaderHidden(true);
    typeTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    typeTree_->setIconSize(QSize(16, 16));
    typeTree_->setMinimumWidth(160);
    nodeList_ = new QListWidget(this);
    nodeList_->setSelectionMode(QAbstractItemView::SingleSelection);
    nodeList_->setMinimumWidth(160);
    listsRow->addWidget(typeTree_, 1);
    listsRow->addWidget(nodeList_, 1);
    vbox->addLayout(listsRow);

    populateTree();

    connect(typeTree_, &QTreeWidget::itemSelectionChanged, this, [this]() {
        populateListForType(selectedNodeType(), filterEdit_->text());
    });
    connect(filterEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
        populateListForType(selectedNodeType(), text);
    });
    connect(addButton_, &QPushButton::clicked, this, [this]() {
        if (nodeList_->selectedItems().isEmpty()) return;
        const NodeType type = selectedNodeType();
        QString templateId;
        QJsonObject taskTemplate;
        if (type == NodeType::Template) {
            templateId = selectedTemplateId();
            if (templateId.isEmpty()) return;
        } else if (type == NodeType::Shell || type == NodeType::Http || type == NodeType::Local) {
            taskTemplate = selectedTaskTemplate();
            templateId = selectedTemplateId();
            if (type == NodeType::Local && taskTemplate.isEmpty()) return;
        }
        emit addNodeRequested(type, templateId, taskTemplate);
    });

    // 默认选中第一个子节点
    if (auto* root = typeTree_->topLevelItemCount() > 0 ? typeTree_->topLevelItem(0) : nullptr) {
        if (root->childCount() > 0) {
            typeTree_->setCurrentItem(root->child(0));
        } else {
            typeTree_->setCurrentItem(root);
        }
    }
}

void NodePaletteWidget::populateTree()
{
    typeTree_->clear();
    auto* root = new QTreeWidgetItem(typeTree_, QStringList() << tr("基础节点"));
    root->setExpanded(true);

    for (const auto& def : kTypeDefs) {
        auto* item = new QTreeWidgetItem(root, QStringList() << def.text);
        item->setData(0, Qt::UserRole, static_cast<int>(def.type));
        item->setIcon(0, QIcon(def.icon));
    }
}

QString NodePaletteWidget::nodeTypeToString(NodeType type) const
{
    switch (type) {
    case NodeType::Shell:
        return QStringLiteral("Shell");
    case NodeType::Http:
        return QStringLiteral("Http");
    case NodeType::Local:
        return QStringLiteral("Local");
    default:
        return QString();
    }
}

void NodePaletteWidget::populateListForType(NodeType type, const QString& filterText)
{
    nodeList_->clear();
    const QString filter = filterText.trimmed();
    if (type == NodeType::Template) {
        if (templates_.isEmpty() && api_ && !awaitingTemplates_) {
            awaitingTemplates_ = true;
            api_->listTemplates();
        }
        for (const auto& v : templates_) {
            if (!v.isObject()) continue;
            const QJsonObject obj = v.toObject();
            const QString templateId = obj.value("template_id").toString();
            const QString name = obj.value("name").toString();
            if (templateId.isEmpty()) continue;
            QString display = name.isEmpty() ? templateId : QString("%1 (%2)").arg(name, templateId);
            if (!filter.isEmpty()
                && !display.contains(filter, Qt::CaseInsensitive)
                && !templateId.contains(filter, Qt::CaseInsensitive)
                && !name.contains(filter, Qt::CaseInsensitive)) {
                continue;
            }
            auto* item = new QListWidgetItem(display, nodeList_);
            item->setData(Qt::UserRole, templateId);
        }
    } else if (type == NodeType::Shell || type == NodeType::Http || type == NodeType::Local) {
        if (taskTemplates_.isEmpty() && api_ && !awaitingTaskTemplates_) {
            awaitingTaskTemplates_ = true;
            api_->listTaskTemplates();
        }
        const QString typeFilter = nodeTypeToString(type);
        for (const auto& v : taskTemplates_) {
            if (!v.isObject()) continue;
            const QJsonObject obj = v.toObject();
            const QString execType = obj.value("exec_type").toString();
            if (!typeFilter.isEmpty() && execType.compare(typeFilter, Qt::CaseInsensitive) != 0) {
                continue;
            }
            const QString templateId = obj.value("template_id").toString();
            const QString name = obj.value("name").toString();
            if (templateId.isEmpty()) continue;
            QString display = name.isEmpty() ? templateId : QString("%1 (%2)").arg(name, templateId);
            if (!filter.isEmpty()
                && !display.contains(filter, Qt::CaseInsensitive)
                && !templateId.contains(filter, Qt::CaseInsensitive)
                && !name.contains(filter, Qt::CaseInsensitive)) {
                continue;
            }
            auto* item = new QListWidgetItem(display, nodeList_);
            item->setData(Qt::UserRole, templateId);
            item->setData(Qt::UserRole + 1, obj);
        }
        if (type != NodeType::Local) {
            const QString name = tr("<自定义节点>");
            if (filter.isEmpty() || name.contains(filter, Qt::CaseInsensitive)) {
                nodeList_->addItem(name);
            }
        }
    } else {
        const QString name = tr("<自定义节点>");
        if (filter.isEmpty() || name.contains(filter, Qt::CaseInsensitive)) {
            nodeList_->addItem(name);
        }
    }

    if (nodeList_->count() > 0) {
        nodeList_->setCurrentRow(0);
    }
}

NodeType NodePaletteWidget::selectedNodeType() const
{
    auto* item = typeTree_->currentItem();
    if (!item) return NodeType::Shell;
    QVariant v = item->data(0, Qt::UserRole);
    if (!v.isValid()) {
        if (item->childCount() > 0) {
            auto* child = item->child(0);
            if (child) return static_cast<NodeType>(child->data(0, Qt::UserRole).toInt());
        }
        return NodeType::Shell;
    }
    return static_cast<NodeType>(v.toInt());
}

QString NodePaletteWidget::selectedTemplateId() const
{
    if (nodeList_->selectedItems().isEmpty()) return QString();
    auto* item = nodeList_->selectedItems().first();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

QJsonObject NodePaletteWidget::selectedTaskTemplate() const
{
    if (nodeList_->selectedItems().isEmpty()) return QJsonObject();
    auto* item = nodeList_->selectedItems().first();
    if (!item) return QJsonObject();
    const QVariant v = item->data(Qt::UserRole + 1);
    if (v.canConvert<QJsonObject>()) {
        return v.toJsonObject();
    }
    return QJsonObject();
}

void NodePaletteWidget::onTemplates(const QJsonArray& items)
{
    templates_ = items;
    awaitingTemplates_ = false;
    if (selectedNodeType() == NodeType::Template) {
        populateListForType(NodeType::Template, filterEdit_ ? filterEdit_->text() : QString());
    }
}

void NodePaletteWidget::onTaskTemplates(const QJsonArray& items)
{
    taskTemplates_ = items;
    awaitingTaskTemplates_ = false;
    const NodeType type = selectedNodeType();
    if (type == NodeType::Shell || type == NodeType::Http || type == NodeType::Local) {
        populateListForType(type, filterEdit_ ? filterEdit_->text() : QString());
    }
}
