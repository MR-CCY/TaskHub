#include "inspector_panel.h"

#include <QStackedWidget>
#include <QFormLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>

#include "view/canvasscene.h"
#include "view/canvasview.h"
#include "commands/undostack.h"
#include "commands/command.h"
#include "Item/rect_item.h"
#include "Item/line_item.h"
#include "Item/line_item_factory.h"

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

    // DAG page
    dagPage_ = new QWidget(stack_);
    auto* dagForm = new QFormLayout(dagPage_);
    dagNameEdit_ = new QLineEdit(dagName_, dagPage_);
    dagFailPolicyCombo_ = new QComboBox(dagPage_);
    dagFailPolicyCombo_->addItem("FailFast");
    dagFailPolicyCombo_->addItem("SkipDownstream");
    dagFailPolicyCombo_->setCurrentText(dagFailPolicy_);
    dagMaxParallelSpin_ = new QSpinBox(dagPage_);
    dagMaxParallelSpin_->setRange(1, 128);
    dagMaxParallelSpin_->setValue(dagMaxParallel_);
    dagSaveBtn_ = new QPushButton(tr("保存 DAG"), dagPage_);
    dagForm->addRow(tr("name"), dagNameEdit_);
    dagForm->addRow(tr("fail_policy"), dagFailPolicyCombo_);
    dagForm->addRow(tr("max_parallel"), dagMaxParallelSpin_);
    dagForm->addRow(dagSaveBtn_);
    connect(dagSaveBtn_, &QPushButton::clicked, this, &InspectorPanel::saveDagEdits);

    // Node page
    nodePage_ = new QWidget(stack_);
    auto* nodeForm = new QFormLayout(nodePage_);
    nodeNameEdit_ = new QLineEdit(nodePage_);
    nodeCmdEdit_ = new QLineEdit(nodePage_);
    nodeTimeoutEdit_ = new QLineEdit(nodePage_);
    nodeRetryEdit_ = new QLineEdit(nodePage_);
    nodeQueueEdit_ = new QLineEdit(nodePage_);
    nodeCaptureBox_ = new QCheckBox(nodePage_);
    httpMethodEdit_ = new QLineEdit(nodePage_);
    httpBodyEdit_ = new QLineEdit(nodePage_);
    shellCwdEdit_ = new QLineEdit(nodePage_);
    shellShellEdit_ = new QLineEdit(nodePage_);
    innerTypeEdit_ = new QLineEdit(nodePage_);
    innerCmdEdit_ = new QLineEdit(nodePage_);
    nodeSaveBtn_ = new QPushButton(tr("保存节点"), nodePage_);
    nodeForm->addRow(tr("name"), nodeNameEdit_);
    nodeForm->addRow(tr("exec_command"), nodeCmdEdit_);
    nodeForm->addRow(tr("timeout_ms"), nodeTimeoutEdit_);
    nodeForm->addRow(tr("retry_count"), nodeRetryEdit_);
    nodeForm->addRow(tr("queue"), nodeQueueEdit_);
    nodeForm->addRow(tr("capture_output"), nodeCaptureBox_);
    nodeForm->addRow(tr("http method"), httpMethodEdit_);
    nodeForm->addRow(tr("http body"), httpBodyEdit_);
    nodeForm->addRow(tr("shell cwd"), shellCwdEdit_);
    nodeForm->addRow(tr("shell shell"), shellShellEdit_);
    nodeForm->addRow(tr("inner exec_type"), innerTypeEdit_);
    nodeForm->addRow(tr("inner exec_command"), innerCmdEdit_);
    nodeForm->addRow(nodeSaveBtn_);
    connect(nodeSaveBtn_, &QPushButton::clicked, this, &InspectorPanel::saveNodeEdits);

    // Line page
    linePage_ = new QWidget(stack_);
    auto* lineForm = new QFormLayout(linePage_);
    lineStartLabel_ = new QLabel("-", linePage_);
    lineEndLabel_ = new QLabel("-", linePage_);
    lineChooseStartBtn_ = new QPushButton(tr("选择起点"), linePage_);
    lineChooseEndBtn_ = new QPushButton(tr("选择终点"), linePage_);
    lineSaveBtn_ = new QPushButton(tr("保存连线"), linePage_);
    lineForm->addRow(tr("起点"), lineStartLabel_);
    lineForm->addRow(lineChooseStartBtn_);
    lineForm->addRow(tr("终点"), lineEndLabel_);
    lineForm->addRow(lineChooseEndBtn_);
    lineForm->addRow(lineSaveBtn_);
    connect(lineChooseStartBtn_, &QPushButton::clicked, this, &InspectorPanel::chooseLineStart);
    connect(lineChooseEndBtn_, &QPushButton::clicked, this, &InspectorPanel::chooseLineEnd);
    connect(lineSaveBtn_, &QPushButton::clicked, this, &InspectorPanel::saveLineEdits);

    stack_->addWidget(dagPage_);
    stack_->addWidget(nodePage_);
    stack_->addWidget(linePage_);
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
    dagNameEdit_->setText(dagName_);
    dagFailPolicyCombo_->setCurrentText(dagFailPolicy_);
    dagMaxParallelSpin_->setValue(dagMaxParallel_);
    stack_->setCurrentWidget(dagPage_);
}

void InspectorPanel::showNode(RectItem* node) {
    QVariantMap props = node->properties();
    QVariantMap exec = props.value("exec_params").toMap();
    nodeNameEdit_->setText(props.value("name").toString());
    nodeCmdEdit_->setText(props.value("exec_command").toString());
    nodeTimeoutEdit_->setText(QString::number(props.value("timeout_ms").toLongLong()));
    nodeRetryEdit_->setText(QString::number(props.value("retry_count").toInt()));
    nodeQueueEdit_->setText(props.value("queue").toString());
    nodeCaptureBox_->setChecked(props.value("capture_output").toBool());
    httpMethodEdit_->setText(exec.value("method").toString());
    httpBodyEdit_->setText(exec.value("body").toString());
    shellCwdEdit_->setText(exec.value("cwd").toString());
    shellShellEdit_->setText(exec.value("shell").toString());
    innerTypeEdit_->setText(exec.value("inner.exec_type").toString());
    innerCmdEdit_->setText(exec.value("inner.exec_command").toString());
    stack_->setCurrentWidget(nodePage_);
}

void InspectorPanel::showLine(LineItem* line) {
    currentLine_ = line;
    pendingLineStart_ = line->startItem();
    pendingLineEnd_ = line->endItem();
    QString startLabel = pendingLineStart_ ? pendingLineStart_->prop("name").toString() : "-";
    QString endLabel = pendingLineEnd_ ? pendingLineEnd_->prop("name").toString() : "-";
    lineStartLabel_->setText(startLabel);
    lineEndLabel_->setText(endLabel);
    stack_->setCurrentWidget(linePage_);
}

void InspectorPanel::saveDagEdits() {
    QString newName = dagNameEdit_->text();
    QString newFail = dagFailPolicyCombo_->currentText();
    int newMax = dagMaxParallelSpin_->value();
    if (!undo_) return;
    undo_->beginMacro("Edit DAG Config");
    auto* cmd = new DagConfigCommand(&dagName_, &dagFailPolicy_, &dagMaxParallel_, dagName_, newName, dagFailPolicy_, newFail, dagMaxParallel_, newMax);
    undo_->push(cmd);
    undo_->endMacro();
    dagName_ = newName;
    dagFailPolicy_ = newFail;
    dagMaxParallel_ = newMax;
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
    pushChange("name", props.value("name"), nodeNameEdit_->text());
    pushChange("exec_command", props.value("exec_command"), nodeCmdEdit_->text());
    pushChange("timeout_ms", props.value("timeout_ms"), nodeTimeoutEdit_->text().toLongLong());
    pushChange("retry_count", props.value("retry_count"), nodeRetryEdit_->text().toInt());
    pushChange("queue", props.value("queue"), nodeQueueEdit_->text());
    pushChange("capture_output", props.value("capture_output"), nodeCaptureBox_->isChecked());
    pushChange("exec_params.method", exec.value("method"), httpMethodEdit_->text());
    pushChange("exec_params.body", exec.value("body"), httpBodyEdit_->text());
    pushChange("exec_params.cwd", exec.value("cwd"), shellCwdEdit_->text());
    pushChange("exec_params.shell", exec.value("shell"), shellShellEdit_->text());
    pushChange("exec_params.inner.exec_type", exec.value("inner.exec_type"), innerTypeEdit_->text());
    pushChange("exec_params.inner.exec_command", exec.value("inner.exec_command"), innerCmdEdit_->text());
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
