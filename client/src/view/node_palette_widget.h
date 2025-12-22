#pragma once
#include <QWidget>
#include "Item/node_type.h"

class QLineEdit;
class QPushButton;
class QTreeWidget;
class QListWidget;

// Simple palette widget: tree of node types + list of nodes with filter and add button.
class NodePaletteWidget : public QWidget {
    Q_OBJECT
public:
    explicit NodePaletteWidget(QWidget* parent = nullptr);
    ~NodePaletteWidget() override = default;

signals:
    void addNodeRequested(NodeType type);

private:
    void buildUi();
    void populateTree();
    void populateListForType(NodeType type, const QString& filterText);
    NodeType selectedNodeType() const;

private:
    QLineEdit* filterEdit_ = nullptr;
    QPushButton* addButton_ = nullptr;
    QTreeWidget* typeTree_ = nullptr;
    QListWidget* nodeList_ = nullptr;
};
