#include <QApplication>
#include <QLabel>
#include <QMainWindow>
#include <QTimer>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QMainWindow window;
    window.setWindowTitle("Code Xray — E0");
    auto* message = new QLabel(QString::fromUtf8("Каркас Code Xray готовий. Модулі A–D ще в розробці."));
    message->setAlignment(Qt::AlignCenter);
    window.setCentralWidget(message);
    window.resize(960, 640);
    window.show();
    if (application.arguments().contains("--smoke-test")) {
        QTimer::singleShot(100, &application, &QApplication::quit);
    }
    return application.exec();
}
