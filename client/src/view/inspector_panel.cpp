#include "inspector_panel.h"

#include <QStackedWidget>
#include <QVBoxLayout>

#include "view/canvasscene.h"
#include "view/canvasview.h"
#include "commands/undostack.h"
#include "commands/command.h"
#include <QSet>
#include "Item/rect_item.h"
#include "Item/line_item.h"
#include "Item/line_item_factory.h"
#include "dag_inspector_widget.h"
#include "node_inspector_widget.h"
#include "line_inspector_widget.h"
#include "dag_serializer.h"
#include "net/api_client.h"
#include <QDebug>

InspectorPanel::InspectorPanel(CanvasScene* scene, UndoStack* undo, CanvasView* view, QWidget* parent)
    : QWidget(parent), scene_(scene), undo_(undo), view_(view) {
    buildUi();
    if (scene_) {
        connect(scene_, &QGraphicsScene::selectionChanged, this, &InspectorPanel::onSelectionChanged);
    }
    onSelectionChanged();
}

void InspectorPanel::buildUi() {
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(6);
    stack_ = new QStackedWidget(this);
    vbox->addWidget(stack_);

    dagWidget_ = new DagInspectorWidget(stack_);
    nodeWidget_ = new NodeInspectorWidget(stack_);
    lineWidget_ = new LineInspectorWidget(stack_);

    connect(dagWidget_, &DagInspectorWidget::saveRequested, this, &InspectorPanel::saveDagEdits);
    connect(dagWidget_, &DagInspectorWidget::runRequested, this, &InspectorPanel::runDag);
    connect(nodeWidget_, &NodeInspectorWidget::saveRequested, this, &InspectorPanel::saveNodeEdits);
    connect(lineWidget_, &LineInspectorWidget::chooseStartRequested, this, &InspectorPanel::chooseLineStart);
    connect(lineWidget_, &LineInspectorWidget::chooseEndRequested, this, &InspectorPanel::chooseLineEnd);
    connect(lineWidget_, &LineInspectorWidget::saveRequested, this, &InspectorPanel::saveLineEdits);

    stack_->addWidget(dagWidget_);
    stack_->addWidget(nodeWidget_);
    stack_->addWidget(lineWidget_);
}

void InspectorPanel::onSelectionChanged() {
    if (!scene_) return;
    auto selected = scene_->selectedItems();
    if (selected.size() == 1) {
        if (auto* node = dynamic_cast<RectItem*>(selected.first())) {
            showNode(node);
            return;
        }
        if (auto* line = dynamic_cast<LineItem*>(selected.first())) {
            showLine(line);
            return;
        }
    }
    showDag();
}

void InspectorPanel::showDag() {
    dagWidget_->setValues(dagName_, dagFailPolicy_, dagMaxParallel_);
    stack_->setCurrentWidget(dagWidget_);
}

void InspectorPanel::showNode(RectItem* node) {
    QVariantMap props = node->properties();
    QVariantMap exec = props.value("exec_params").toMap();
    nodeWidget_->setValues(props, exec);
    stack_->setCurrentWidget(nodeWidget_);
}

void InspectorPanel::showLine(LineItem* line) {
    currentLine_ = line;
    pendingLineStart_ = line->startItem();
    pendingLineEnd_ = line->endItem();
    QString startLabel = pendingLineStart_ ? pendingLineStart_->prop("name").toString() : "-";
    QString endLabel = pendingLineEnd_ ? pendingLineEnd_->prop("name").toString() : "-";
    lineWidget_->setEndpoints(startLabel, endLabel);
    stack_->setCurrentWidget(lineWidget_);
}

void InspectorPanel::saveDagEdits() {
    QString newName = dagWidget_->nameValue();
    QString newFail = dagWidget_->failPolicyValue();
    int newMax = dagWidget_->maxParallelValue();
    if (!undo_) return;
    undo_->beginMacro("Edit DAG Config");
    auto* cmd = new DagConfigCommand(&dagName_, &dagFailPolicy_, &dagMaxParallel_, dagName_, newName, dagFailPolicy_, newFail, dagMaxParallel_, newMax);
    undo_->push(cmd);
    undo_->endMacro();
    dagName_ = newName;
    dagFailPolicy_ = newFail;
    dagMaxParallel_ = newMax;
}

void InspectorPanel::runDag() {
    if (!scene_) return;
    dagName_ = dagWidget_->nameValue();
    dagFailPolicy_ = dagWidget_->failPolicyValue();
    dagMaxParallel_ = dagWidget_->maxParallelValue();
    const QJsonObject dagJson = buildDagJson(scene_, dagFailPolicy_, dagMaxParallel_, dagName_);
    if (dagJson.isEmpty()) return;
    if (!api_) {
        qWarning("ApiClient is null, cannot run DAG");
        return;
    }
    api_->runDagAsync(dagJson);
}

void InspectorPanel::saveNodeEdits() {
    if (!scene_ || !undo_) return;
    auto selected = scene_->selectedItems();
    RectItem* node = nullptr;
    for (auto* it : selected) {
        if (auto* r = dynamic_cast<RectItem*>(it)) { node = r; break; }
    }
    if (!node) return;
    QVariantMap props = node->properties();
    QVariantMap exec = props.value("exec_params").toMap();
    undo_->beginMacro("Edit Node");
    auto pushChange = [&](const QString& key, const QVariant& oldV, const QVariant& newV) {
        if (oldV == newV) return;
        auto* cmd = new SetPropertyCommand(node, key, oldV, newV);
        undo_->push(cmd);
    };
    pushChange("name", props.value("name"), nodeWidget_->nameValue());
    pushChange("exec_command", props.value("exec_command"), nodeWidget_->commandValue());
    pushChange("timeout_ms", props.value("timeout_ms"), nodeWidget_->timeoutMsValue());
    pushChange("retry_count", props.value("retry_count"), nodeWidget_->retryCountValue());
    pushChange("queue", props.value("queue"), nodeWidget_->queueValue());
    pushChange("capture_output", props.value("capture_output"), nodeWidget_->captureOutputValue());
    pushChange("exec_params.method", exec.value("method"), nodeWidget_->httpMethodValue());
    pushChange("exec_params.body", exec.value("body"), nodeWidget_->httpBodyValue());
    pushChange("exec_params.cwd", exec.value("cwd"), nodeWidget_->shellCwdValue());
    pushChange("exec_params.shell", exec.value("shell"), nodeWidget_->shellShellValue());
    pushChange("exec_params.inner.exec_type", exec.value("inner.exec_type"), nodeWidget_->innerExecTypeValue());
    pushChange("exec_params.inner.exec_command", exec.value("inner.exec_command"), nodeWidget_->innerExecCommandValue());
    undo_->endMacro();
    onSelectionChanged();
}

void InspectorPanel::chooseLineStart() { pickingStart_ = true; pickingEnd_ = false; }
void InspectorPanel::chooseLineEnd() { pickingEnd_ = true; pickingStart_ = false; }

void InspectorPanel::saveLineEdits() {
    if (!currentLine_ || !scene_ || !undo_) return;
    auto* newStart = pendingLineStart_;
    auto* newEnd = pendingLineEnd_;
    if (!newStart || !newEnd) return;
    if (newStart == currentLine_->startItem() && newEnd == currentLine_->endItem()) return;
    undo_->beginMacro("Reconnect Line");
    QSet<QGraphicsItem*> delItems;
    delItems.insert(currentLine_);
    auto* delCmd = new DeleteCommand(scene_, delItems);
    undo_->push(delCmd);
    auto* line = LineItemFactory::createLine(newStart, newEnd);
    line->attachContext(scene_, nullptr, undo_);
    line->execCreateCmd(true);
    undo_->endMacro();
    currentLine_ = nullptr;
    onSelectionChanged();
}
