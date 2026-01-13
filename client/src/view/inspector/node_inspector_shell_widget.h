#pragma once
#include <QWidget>
#include <QVariant>

class QLabel;
class QLineEdit;

class NodeInspectorShellWidget : public QWidget {
public:
    explicit NodeInspectorShellWidget(QWidget* parent = nullptr);

    void setValues(const QVariantMap& exec);
    QString cwdValue() const;
    QString shellValue() const;
    void setReadOnlyMode(bool ro);
    QString envValue() const;
    void setEnvValue(const QString& env);
    void setCmdValue(const QString& cmd);
    QString cmdValue() const;

private:
    QLabel* cwdLabel_ = nullptr;
    QLabel* shellLabel_ = nullptr;
    QLineEdit* cwdEdit_ = nullptr;
    QLineEdit* shellEdit_ = nullptr;
    QLineEdit* envEdit_ = nullptr;
    QLabel* envLabel_ = nullptr;
    QLineEdit* cmdEdit_ = nullptr;
    QLabel* cmdLabel_ = nullptr;
};
