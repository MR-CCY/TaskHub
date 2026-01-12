#include "dag_edit_bench.h"

#include <QAction>
#include <QKeySequence>
#include <QIcon>
#include <QSize>
#include <QSplitter>
#include <QToolBar>
#include <QVBoxLayout>

#include "view/inspector_panel.h"
#include "view/node_palette_widget.h"
#include "Item/node_type.h"
#include "commands/undostack.h"

DagEditBench::DagEditBench(QWidget* parent)
    : CanvasBench(parent)
{
    buildUi();
    wireUi();
}

void DagEditBench::setApiClient(ApiClient* api)
{
    api_ = api;
    if (inspector_) inspector_->setApiClient(api);
    if (nodePanel_) nodePanel_->setApiClient(api);
}
void DagEditBench::buildUi()
{
    toolbar_ = new QToolBar(this);
    toolbar_->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    toolbar_->setIconSize(QSize(24, 24));
    actUndo_ = toolbar_->addAction(QIcon(":/icons/image/undo.png"), tr("撤销"));
    actRedo_ = toolbar_->addAction(QIcon(":/icons/image/do.png"), tr("重做"));

    actConnect_ = toolbar_->addAction(QIcon(":/icons/image/link.png"), tr("连线"));
    actDelete_  = toolbar_->addAction(QIcon(":/icons/image/del.png"), tr("删除"));
    actExportJson_ = toolbar_->addAction(QIcon(":/icons/image/daochu.png"), tr("导出 json"));
    actImportJson_ = toolbar_->addAction(QIcon(":/icons/image/daoru.png"), tr("导入 json"));
    actLayout_ = toolbar_->addAction(QIcon(":/icons/image/layout.png"),tr("布局"));

    actUndo_->setShortcut(QKeySequence::Undo);
    actRedo_->setShortcut(QKeySequence::Redo);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(toolbar_);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setContentsMargins(0, 0, 0, 0);
    splitter->setHandleWidth(4);
    nodePanel_ = new NodePaletteWidget(this);
    splitter->addWidget(nodePanel_);
    splitter->addWidget(view());
    inspector_ = new InspectorPanel(scene(), undoStack(), view(), this);
    inspector_->setApiClient(api_);
    splitter->addWidget(inspector_);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 4);
    splitter->setStretchFactor(2, 1);
    layout->addWidget(splitter);
}

void DagEditBench::wireUi()
{
    connect(actUndo_, &QAction::triggered, this, &DagEditBench::undo);
    connect(actRedo_, &QAction::triggered, this, &DagEditBench::redo);
    connect(actExportJson_, &QAction::triggered, this, &DagEditBench::exportJson);
    connect(actImportJson_, &QAction::triggered, this, &DagEditBench::importJson);
    connect(actLayout_, &QAction::triggered, this, &DagEditBench::layoutDag);

    if (auto* stack = undoStack()->internalStack()) {
        connect(stack, &QUndoStack::canUndoChanged, actUndo_, &QAction::setEnabled);
        connect(stack, &QUndoStack::canRedoChanged, actRedo_, &QAction::setEnabled);
        actUndo_->setEnabled(stack->canUndo());
        actRedo_->setEnabled(stack->canRedo());
    }

    connect(actConnect_, &QAction::triggered, this, &DagEditBench::startConnectMode);
    connect(actDelete_, &QAction::triggered, this, &DagEditBench::deleteSelected);
    connect(nodePanel_, &NodePaletteWidget::addNodeRequested, this, [this](NodeType type, const QString& templateId) {
        startCreateNodeMode(type, templateId);
    });
}
