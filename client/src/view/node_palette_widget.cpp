#include "node_palette_widget.h"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStyle>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
const struct ItemDef { QString text; NodeType type; QString icon; } kTypeDefs[] = {
    { QObject::tr("Shell 节点"),  NodeType::Shell,  QStringLiteral(":/icons/image/shell.png")  },
    { QObject::tr("HTTP 节点"),   NodeType::Http,   QStringLiteral(":/icons/image/http.png")   },
    { QObject::tr("Local 节点"),  NodeType::Local,  QStringLiteral(":/icons/image/local.png")  },
    { QObject::tr("Remote 节点"), NodeType::Remote, QStringLiteral(":/icons/image/remote.png") },
    { QObject::tr("DAG 节点"),    NodeType::Dag,    QStringLiteral(":/icons/image/layout.png") },
    { QObject::tr("模版 节点"),   NodeType::Template, QStringLiteral(":/icons/image/layout.png") },
};
}

NodePaletteWidget::NodePaletteWidget(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
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
        emit addNodeRequested(selectedNodeType());
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

void NodePaletteWidget::populateListForType(NodeType type, const QString& filterText)
{
    nodeList_->clear();
    const QString filter = filterText.trimmed();
    QStringList items;
    items << tr("<自定义节点>");

    for (const QString& name : items) {
        if (!filter.isEmpty() && !name.contains(filter, Qt::CaseInsensitive)) {
            continue;
        }
        nodeList_->addItem(name);
    }

    if (nodeList_->count() > 0) {
        nodeList_->setCurrentRow(0);
    }
    Q_UNUSED(type);
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
