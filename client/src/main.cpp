// Minimal Qt client entry to verify Qt wiring builds and shows a window.
#include <QApplication>
#include <QWidget>
#include <functional>
#include "login_dialog.h"
#include "app_context.h"
#include "main_window.h"
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    // app.setQuitOnLastWindowClosed(false); // keep app alive while we re-prompt login

    std::function<void()> showMain;
    showMain = [&]() {
        MainWindow* mainw = new MainWindow();
        QObject::connect(mainw, &MainWindow::logoutRequested, [&]() {
            if (!AppContext::instance().isLoggedIn()) {
                LoginDialog login;
                login.show();
                if (login.exec() == QDialog::Accepted) {
                    showMain();
                } else {
                    QApplication::quit();
                }
            } else {
                QApplication::quit();
            }
        });
        mainw->show();
    };

    LoginDialog login;
    if (login.exec() == QDialog::Accepted) {
        showMain();
    } else {
        return 0;
    }

    return app.exec();
}
