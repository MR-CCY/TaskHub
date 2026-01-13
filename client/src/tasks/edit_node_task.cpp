#include "edit_node_task.h"

#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QEvent>
#include <QMessageBox>

#include "view/canvas/canvasscene.h"
#include "view/canvas/canvasview.h"
#include "commands/undostack.h"
#include "commands/command.h"
#include "Item/rect_item.h"
#include "Item/node_type.h"

namespace {
class AutoCloseDialog : public QDialog {
public:
    explicit AutoCloseDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowFlag(Qt::Tool);
    }
protected:
    bool event(QEvent* e) override {
        if (e->type() == QEvent::FocusOut) {
            // 失焦时自动结束对话框，但不销毁（避免 exec() 返回后访问空指针）
            reject();
        }
        return QDialog::event(e);
    }
};
}

EditNodeTask::EditNodeTask(CanvasScene* scene, UndoStack* undo, CanvasView* view, QWidget* parentWidget, QObject* parent)
    : Task(50, parent), scene_(scene), undo_(undo), view_(view), parentWidget_(parentWidget) {}

bool EditNodeTask::dispatch(QEvent* e) {
    Q_UNUSED(e);
    return false;
}

void EditNodeTask::execute() {
    if (!scene_ || !undo_) {
        removeSelf();
        return;
    }
    auto selected = scene_->selectedItems();
    RectItem* node = nullptr;
    for (auto* it : selected) {
        if (auto* r = dynamic_cast<RectItem*>(it)) {
            node = r;
            break;
        }
    }
    if (!node) {
        QMessageBox::information(parentWidget_, tr("提示"), tr("请先选中一个节点"));
        removeSelf();
        return;
    }

    QVariantMap props = node->properties();
    QVariantMap execParams = props.value("exec_params").toMap();
    const QString execType = props.value("exec_type").toString();

    auto* dlg = new AutoCloseDialog(parentWidget_);
    dlg->setWindowTitle(tr("编辑节点属性"));
    auto* form = new QFormLayout(dlg);

    QLineEdit* nameEdit = new QLineEdit(props.value("name").toString(), dlg);
    QLineEdit* cmdEdit = new QLineEdit(props.value("exec_command").toString(), dlg);
    QLineEdit* timeoutEdit = new QLineEdit(QString::number(props.value("timeout_ms").toLongLong()), dlg);
    QLineEdit* retryCountEdit = new QLineEdit(QString::number(props.value("retry_count").toInt()), dlg);
    QLineEdit* queueEdit = new QLineEdit(props.value("queue").toString(), dlg);
    QCheckBox* captureBox = new QCheckBox(dlg);
    captureBox->setChecked(props.value("capture_output").toBool());

    form->addRow(tr("name"), nameEdit);
    form->addRow(tr("exec_command"), cmdEdit);
    form->addRow(tr("timeout_ms"), timeoutEdit);
    form->addRow(tr("retry_count"), retryCountEdit);
    form->addRow(tr("queue"), queueEdit);
    form->addRow(tr("capture_output"), captureBox);

    QLineEdit* httpMethod = nullptr;
    QLineEdit* httpBody = nullptr;
    QLineEdit* shellCwd = nullptr;
    QLineEdit* shellShell = nullptr;

    if (execType.compare("HttpCall", Qt::CaseInsensitive) == 0) {
        httpMethod = new QLineEdit(execParams.value("method").toString(), dlg);
        httpBody = new QLineEdit(execParams.value("body").toString(), dlg);
        form->addRow(tr("http method"), httpMethod);
        form->addRow(tr("http body"), httpBody);
    } else if (execType.compare("Shell", Qt::CaseInsensitive) == 0) {
        shellCwd = new QLineEdit(execParams.value("cwd").toString(), dlg);
        shellShell = new QLineEdit(execParams.value("shell").toString(), dlg);
        form->addRow(tr("cwd"), shellCwd);
        form->addRow(tr("shell"), shellShell);
    } else if (execType.compare("Remote", Qt::CaseInsensitive) == 0) {
        // Remote 节点目前在 Inspector 面板中处理队列选择，不再此处编辑 inner 任务
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    form->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    if (dlg->exec() == QDialog::Accepted) {
        undo_->beginMacro("Edit Node");
        auto pushChange = [&](const QString& key, const QVariant& oldV, const QVariant& newV) {
            if (oldV == newV) return;
            auto* cmd = new SetPropertyCommand(node, key, oldV, newV);
            undo_->push(cmd);
        };

        pushChange("name", props.value("name"), nameEdit->text());
        pushChange("exec_command", props.value("exec_command"), cmdEdit->text());
        pushChange("timeout_ms", props.value("timeout_ms"), timeoutEdit->text().toLongLong());
        pushChange("retry_count", props.value("retry_count"), retryCountEdit->text().toInt());
        pushChange("queue", props.value("queue"), queueEdit->text());
        pushChange("capture_output", props.value("capture_output"), captureBox->isChecked());

        if (httpMethod) {
            pushChange("exec_params.method", execParams.value("method"), httpMethod->text());
            pushChange("exec_params.body", execParams.value("body"), httpBody->text());
        }
        if (shellCwd) {
            pushChange("exec_params.cwd", execParams.value("cwd"), shellCwd->text());
            pushChange("exec_params.shell", execParams.value("shell"), shellShell->text());
        }

        undo_->endMacro();
    }

    dlg->deleteLater(); // TODO: 后续升级为右侧 Inspector 面板
    removeSelf();
}
