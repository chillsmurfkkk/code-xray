#include "main_window.hpp"

#include <QApplication>
#include <QTimer>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);

    xray::app::MainWindow window;
    window.show();

    if (application.arguments().contains("--smoke-test")) {
        QTimer::singleShot(100, &application, &QApplication::quit);
    }

    return application.exec();
}
