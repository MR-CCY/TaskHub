#pragma once
#include <QJsonArray>
#include <QWidget>
#include "Item/node_type.h"

class ApiClient;
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
    void setApiClient(ApiClient* api);

signals:
    void addNodeRequested(NodeType type, const QString& templateId);

private:
    void buildUi();
    void populateTree();
    void populateListForType(NodeType type, const QString& filterText);
    NodeType selectedNodeType() const;
    QString selectedTemplateId() const;

private slots:
    void onTemplates(const QJsonArray& items);

private:
    ApiClient* api_ = nullptr;
    QJsonArray templates_;
    bool awaitingTemplates_ = false;
    QLineEdit* filterEdit_ = nullptr;
    QPushButton* addButton_ = nullptr;
    QTreeWidget* typeTree_ = nullptr;
    QListWidget* nodeList_ = nullptr;
};
