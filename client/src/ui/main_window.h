#pragma once
#include <QMainWindow>
#include <QUrl>
#include <QEvent>

class ApiClient;
class WsClient;
class ConsoleDock;
class DagEditBench;
class UserBarWidget;
class QListWidget;
class QStackedWidget;
class QDockWidget;
class InspectorPanel;
class DagRunsWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
signals:
    void logoutRequested(); // 当需要重新登录时发出
private slots:
    void onUnauthorized();
    void onLoginOk(const QString& token, const QString& username);
    void onNavIndexChanged(int index);
protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
private:
    void setupUserBar();
    void setupCentralPages();
    void setupNavDock();
    void setupConsole();
    void setupClients();
    void wireSignals();
    QWidget* createPlaceholderPage(const QString& text);

    UserBarWidget* userBar_ = nullptr;
    QStackedWidget* centralStack_ = nullptr;
    QListWidget* navList_ = nullptr;
    QDockWidget* navDock_ = nullptr;

    DagEditBench* workflowPage_ = nullptr;
    QWidget* templatesPage_ = nullptr;
    DagRunsWidget* runsPage_ = nullptr;
    QWidget* cronPage_ = nullptr;

    ConsoleDock* console_ = nullptr;
    ApiClient* api_ = nullptr;
    WsClient* ws_ = nullptr;

    QUrl wsUrl_= QUrl("ws://127.0.0.1:8090/ws"); 
};
