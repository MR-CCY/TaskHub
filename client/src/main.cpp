// Minimal Qt client entry to verify Qt wiring builds and shows a window.
#include <QApplication>
#include <QWidget>
#include "login_dialog.h"
#include "ui/main_window.h"
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    LoginDialog* loginw=new LoginDialog();
    if(loginw->exec()!=QDialog::Accepted){
        return 0;
    }
    MainWindow* mainw=new MainWindow(); 
    mainw->show();
    return app.exec();
}
