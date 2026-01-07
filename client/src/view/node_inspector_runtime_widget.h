#pragma once
#include <QWidget>

class QJsonObject;
class QLabel;

class NodeInspectorRuntimeWidget : public QWidget {
public:
    explicit NodeInspectorRuntimeWidget(QWidget* parent = nullptr);

    void setRuntimeValues(const QJsonObject& obj);

private:
    QLabel* runtimeStatusLabel_ = nullptr;
    QLabel* runtimeDurationLabel_ = nullptr;
    QLabel* runtimeExitLabel_ = nullptr;
    QLabel* runtimeAttemptLabel_ = nullptr;
    QLabel* runtimeWorkerLabel_ = nullptr;
    QLabel* runtimeMessageLabel_ = nullptr;
    QLabel* runtimeStdoutLabel_ = nullptr;
    QLabel* runtimeStderrLabel_ = nullptr;
};
