// Minimal Qt client entry to verify Qt wiring builds and shows a window.
#include <QApplication>
#include <QWidget>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle("TaskHub Client");
    window.resize(480, 320);
    window.show();

    return app.exec();
}
