#include "node_inspector_runtime_widget.h"

#include <QFormLayout>
#include <QFrame>
#include <QJsonObject>
#include <QLabel>

NodeInspectorRuntimeWidget::NodeInspectorRuntimeWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* form = new QFormLayout(this);
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(6);

    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);

    runtimeStatusLabel_ = new QLabel("-", this);
    runtimeDurationLabel_ = new QLabel("-", this);
    runtimeExitLabel_ = new QLabel("-", this);
    runtimeAttemptLabel_ = new QLabel("-", this);
    runtimeWorkerLabel_ = new QLabel("-", this);
    runtimeMessageLabel_ = new QLabel("-", this);
    runtimeStdoutLabel_ = new QLabel("-", this);
    runtimeStdoutLabel_->setWordWrap(true);
    runtimeStderrLabel_ = new QLabel("-", this);
    runtimeStderrLabel_->setWordWrap(true);

    form->addRow(sep);
    form->addRow(tr("运行状态"), runtimeStatusLabel_);
    form->addRow(tr("耗时(ms)"), runtimeDurationLabel_);
    form->addRow(tr("退出码"), runtimeExitLabel_);
    form->addRow(tr("尝试/最大"), runtimeAttemptLabel_);
    form->addRow(tr("Worker"), runtimeWorkerLabel_);
    form->addRow(tr("消息"), runtimeMessageLabel_);
    form->addRow(tr("Stdout"), runtimeStdoutLabel_);
    form->addRow(tr("Stderr"), runtimeStderrLabel_);
}

void NodeInspectorRuntimeWidget::setRuntimeValues(const QJsonObject& obj)
{
    const QString status = obj.value("status").toString();
    const qint64 duration = obj.value("duration_ms").toVariant().toLongLong();
    const int exitCode = obj.value("exit_code").toInt();
    const QString worker = obj.value("worker_id").toString();
    const QString stdout = obj.value("stdout").toString();
    const QString stderr = obj.value("stderr").toString();
    const QString msg = obj.value("message").toString();
    const int attempt = obj.value("attempt").toInt();
    const int maxAttempts = obj.value("max_attempts").toInt();

    runtimeStatusLabel_->setText(status);
    runtimeDurationLabel_->setText(QString::number(duration));
    runtimeExitLabel_->setText(QString::number(exitCode));
    runtimeAttemptLabel_->setText(QString("%1 / %2").arg(attempt).arg(maxAttempts));
    runtimeWorkerLabel_->setText(worker);
    runtimeMessageLabel_->setText(msg);
    runtimeStdoutLabel_->setText(stdout);
    runtimeStderrLabel_->setText(stderr);
}
