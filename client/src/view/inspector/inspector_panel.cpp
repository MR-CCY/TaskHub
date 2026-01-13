#include "inspector_panel.h"

#include <QDateTime> 
#include <QUuid>
#include <functional>
#include <QStackedWidget>
#include "utils/id_utils.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QVariant>

#include "view/canvas/canvasscene.h"
#include "view/canvas/canvasview.h"
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
#include <QJsonArray>
#include <QJsonObject>
#include <QPushButton>
namespace {
QJsonObject variantMapToStringObject(const QVariantMap& m) {
    QJsonObject obj;
    for (auto it = m.constBegin(); it != m.constEnd(); ++it) {
        obj[it.key()] = it.value().toString();
    }
    return obj;
}
}

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
    connect(dagWidget_, &DagInspectorWidget::cronCreateRequested, this, &InspectorPanel::createDagCron);
    connect(nodeWidget_, &NodeInspectorWidget::saveRequested, this, &InspectorPanel::saveNodeEdits);
    connect(nodeWidget_, &NodeInspectorWidget::cronCreateRequested, this, &InspectorPanel::createNodeCron);
    connect(lineWidget_, &LineInspectorWidget::chooseStartRequested, this, &InspectorPanel::chooseLineStart);
    connect(lineWidget_, &LineInspectorWidget::chooseEndRequested, this, &InspectorPanel::chooseLineEnd);
    connect(lineWidget_, &LineInspectorWidget::saveRequested, this, &InspectorPanel::saveLineEdits);

    stack_->addWidget(dagWidget_);
    stack_->addWidget(nodeWidget_);
    stack_->addWidget(lineWidget_);

    const int maxWidth = qMax(dagWidget_->sizeHint().width(),
                              qMax(nodeWidget_->sizeHint().width(), lineWidget_->sizeHint().width()));
    if (maxWidth > 0) {
        stack_->setMinimumWidth(maxWidth);
        setMinimumWidth(maxWidth);
    }
}
void InspectorPanel::setReadOnlyMode(bool ro)
{
    if (nodeWidget_) nodeWidget_->setReadOnlyMode(ro);
}

void InspectorPanel::setApiClient(ApiClient* api)
{
    api_ = api;
    if (nodeWidget_) nodeWidget_->setApiClient(api);
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

void InspectorPanel::refreshCurrent()
{
    RectItem* currentNode = nullptr;
    if (scene_ && !scene_->selectedItems().isEmpty()) {
        currentNode = dynamic_cast<RectItem*>(scene_->selectedItems().first());
    }
    if (!currentNode) return;
    showNode(currentNode);
}

void InspectorPanel::showDag() {
    dagWidget_->setValues(dagName_, dagFailPolicy_, dagMaxParallel_);
    stack_->setCurrentWidget(dagWidget_);
}

void InspectorPanel::showNode(RectItem* node) {
    QVariantMap props = node->properties();
    QVariantMap exec = props.value("exec_params").toMap();
    QVariantMap result = props.value("result").toMap();
    nodeWidget_->setValues(props, exec);
    nodeWidget_->setResultValues(result);
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
    QString topRunId = taskhub::utils::generateRunId();
    QJsonObject mutableDag = dagJson;
    mutableDag["run_id"] = topRunId;

    // Recursive helper to inject IDs
    std::function<void(QJsonObject&, const QString&)> processIds;
    processIds = [&processIds](QJsonObject& container, const QString& currentRunId) 
    {
        if (!container.contains("tasks") || !container["tasks"].isArray()) return;
        
        QJsonArray tasks = container["tasks"].toArray();
        for (int i = 0; i < tasks.size(); ++i) {
            QJsonObject task = tasks[i].toObject();
            QString type = task.value("exec_type").toString();
            QJsonObject params = task.value("exec_params").toObject();

            if (type == "Dag" || type == "Template"|| type == "Remote") {
                QString newId = taskhub::utils::generateRunId();
                params["run_id"] = newId;
                processIds(params, newId);
            }
            
            task["exec_params"] = params;
            tasks[i] = task;
        }
        container["tasks"] = tasks;
    };

    processIds(mutableDag, topRunId);

    api_->runDagAsync(mutableDag);
}

void InspectorPanel::createDagCron(const QString& spec)
{
    if (!api_ || spec.isEmpty()) return;
    dagName_ = dagWidget_->nameValue();
    dagFailPolicy_ = dagWidget_->failPolicyValue();
    dagMaxParallel_ = dagWidget_->maxParallelValue();
    const QJsonObject dagJson = buildDagJson(scene_, dagFailPolicy_, dagMaxParallel_, dagName_);
    if (dagJson.isEmpty()) return;

    QJsonObject body;
    body["name"] = dagName_;
    body["spec"] = spec;
    body["target_type"] = "Dag";
    QJsonObject dagObj;
    dagObj["config"] = dagJson.value("config").toObject();
    dagObj["tasks"] = dagJson.value("tasks").toArray();
    body["dag"] = dagObj;
    api_->createCronJob(body);
}

void InspectorPanel::createNodeCron(const QString& spec)
{
    if (!api_ || spec.isEmpty() || !scene_) return;
    RectItem* node = nullptr;
    for (auto* it : scene_->selectedItems()) {
        if (auto* r = dynamic_cast<RectItem*>(it)) { node = r; break; }
    }
    if (!node) return;
    const QVariantMap props = node->properties();
    QJsonObject task;
    static const char* keys[] = {
        "id", "name", "exec_type", "exec_command", "timeout_ms", "retry_count",
        "retry_delay_ms", "retry_exp_backoff", "priority", "capture_output","description"
    };
    for (auto* key : keys) {
        if (!props.contains(key)) continue;
        const QString sk(key);
        const QVariant v = props.value(key);
        if (sk == "timeout_ms" || sk == "retry_delay_ms") {
            task[sk] = static_cast<qint64>(v.toLongLong());
        } else if (sk == "retry_count" || sk == "priority") {
            task[sk] = v.toInt();
        } else if (sk == "retry_exp_backoff" || sk == "capture_output") {
            task[sk] = v.toBool();
        } else {
            task[sk] = v.toString();
        }
    }
    task["exec_params"] = variantMapToStringObject(props.value("exec_params").toMap());
    task["metadata"] = variantMapToStringObject(props.value("metadata").toMap());

    QJsonObject body;
    body["name"] = props.value("name").toString();
    body["spec"] = spec;
    body["target_type"] = "SingleTask";
    body["task"] = task;
    api_->createCronJob(body);
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
    const QString execType = props.value("exec_type").toString().trimmed().toLower();
    QJsonObject templateParams;
    if (execType == "template") {
        if (!nodeWidget_->buildTemplateParamsPayload(templateParams)) {
            return;
        }
    }
    undo_->beginMacro("Edit Node");
    auto pushChange = [&](const QString& key, const QVariant& oldV, const QVariant& newV) {
        if (oldV == newV) return;
        auto* cmd = new SetPropertyCommand(node, key, oldV, newV);
        undo_->push(cmd);
    };
    pushChange("name", props.value("name"), nodeWidget_->nameValue());
    pushChange("description", props.value("description"), nodeWidget_->descriptionValue());
    pushChange("timeout_ms", props.value("timeout_ms"), nodeWidget_->timeoutMsValue());
    pushChange("retry_count", props.value("retry_count"), nodeWidget_->retryCountValue());
    pushChange("priority", props.value("priority"), nodeWidget_->priorityValue());
    pushChange("queue", props.value("queue", "default"), nodeWidget_->queueValue());
    pushChange("capture_output", props.value("capture_output"), nodeWidget_->captureOutputValue());

    pushChange("exec_params.method", exec.value("method"), nodeWidget_->httpMethodValue());
    pushChange("exec_params.url", exec.value("url"), nodeWidget_->httpUrlValue());
    pushChange("exec_params.body", exec.value("body"), nodeWidget_->httpBodyValue());
    pushChange("exec_params.auth.user", exec.value("auth.user"), nodeWidget_->httpAuthUserValue());
    pushChange("exec_params.auth.pass", exec.value("auth.pass"), nodeWidget_->httpAuthPassValue());
    pushChange("exec_params.follow_redirects", exec.value("follow_redirects", "true"), nodeWidget_->httpFollowRedirectsValue() ? "true" : "false");

    // 处理 Headers (解析多行文本为多个 exec_params.header.XXX 项)
    {
        QString oldHeadersText;
        QStringList oldLines;
        for (auto it = exec.begin(); it != exec.end(); ++it) {
            if (it.key().startsWith("header.")) {
                oldLines << QString("%1: %2").arg(it.key().mid(7)).arg(it.value().toString());
            }
        }
        oldHeadersText = oldLines.join("\n");
        QString newHeadersText = nodeWidget_->httpHeadersValue();

        if (oldHeadersText != newHeadersText) {
            // 简单处理：先移除旧的 header.*，再添加新的
            // 注意：这里需要 undo 系统支持批量或特殊处理
            // 为简化演示，我们对比后再分别 push
            
            // 1) 找出所有旧键名并清空（防止残留）
            for (auto it = exec.begin(); it != exec.end(); ++it) {
                if (it.key().startsWith("header.")) {
                    pushChange("exec_params." + it.key(), it.value(), QVariant()); 
                }
            }

            // 2) 写入新键名
            QStringList newLines = newHeadersText.split('\n', Qt::SkipEmptyParts);
            for (const QString& line : newLines) {
                int colon = line.indexOf(':');
                if (colon > 0) {
                    QString k = line.left(colon).trimmed();
                    QString v = line.mid(colon + 1).trimmed();
                    if (!k.isEmpty()) {
                        pushChange("exec_params.header." + k, QVariant(), v);
                    }
                }
            }
        }
    }

    //Shell参数包中的参数
    pushChange("exec_params.cwd", exec.value("cwd"), nodeWidget_->shellCwdValue());
    pushChange("exec_params.shell", exec.value("shell"), nodeWidget_->shellShellValue());
    pushChange("exec_params.env", exec.value("env"), nodeWidget_->shellENVValue());
    pushChange("exec_params.cmd", exec.value("cmd"), nodeWidget_->shellCmdValue());

    // Local 处理器
    pushChange("exec_params.handler", exec.value("handler"), nodeWidget_->localHandlerValue());

    if (execType == "dag") {
        pushChange("exec_params.config.fail_policy", exec.value("config.fail_policy"), nodeWidget_->dagFailPolicyValue());
        pushChange("exec_params.config.max_parallel", exec.value("config.max_parallel"), nodeWidget_->dagMaxParallelValue());
    } else if (execType == "template") {
        pushChange("exec_params.template_id", exec.value("template_id"), nodeWidget_->templateIdValue());
        pushChange("exec_params.params", exec.value("params"), templateParams.toVariantMap());
    } else if (execType == "remote") {
        pushChange("exec_params.config.fail_policy", exec.value("config.fail_policy"), nodeWidget_->remoteFailPolicyValue());
        pushChange("exec_params.config.max_parallel", exec.value("config.max_parallel"), nodeWidget_->remoteMaxParallelValue());
    }
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
    QGraphicsItem* parent = nullptr;
    if (!LineItemFactory::canConnect(newStart, newEnd, parent)) return;
    undo_->beginMacro("Reconnect Line");
    QSet<QGraphicsItem*> delItems;
    delItems.insert(currentLine_);
    auto* delCmd = new DeleteCommand(scene_, delItems);
    undo_->push(delCmd);
    auto* line = LineItemFactory::createLine(newStart, newEnd, parent);
    line->attachContext(scene_, nullptr, undo_);
    line->execCreateCmd(true);
    undo_->endMacro();
    currentLine_ = nullptr;
    onSelectionChanged();
}
