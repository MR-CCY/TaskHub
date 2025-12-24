#pragma once
#include <QWidget>

class QLineEdit;
class QComboBox;
class QSpinBox;
class QPushButton;

// DAG-level inspector form.
class DagInspectorWidget : public QWidget {
    Q_OBJECT
public:
    explicit DagInspectorWidget(QWidget* parent = nullptr);

    void setValues(const QString& name, const QString& failPolicy, int maxParallel);
    QString nameValue() const;
    QString failPolicyValue() const;
    int maxParallelValue() const;

signals:
    void saveRequested();
    void runRequested();

private:
    void buildUi();

private:
    QLineEdit* nameEdit_ = nullptr;
    QComboBox* failPolicyCombo_ = nullptr;
    QSpinBox* maxParallelSpin_ = nullptr;
    QPushButton* saveBtn_ = nullptr;
    QPushButton* runBtn_ = nullptr;
};
