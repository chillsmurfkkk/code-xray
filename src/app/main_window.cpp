#include "main_window.hpp"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace xray::app {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("Code Xray");
    resize(960, 640);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);

    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto* message = new QLabel(
        QString::fromUtf8("Тут буде структура C++ проєкту"),
        central
    );
    message->setAlignment(Qt::AlignCenter);

    auto* button = new QPushButton(
        QString::fromUtf8("Перевірити взаємодію"),
        central
    );

    layout->addWidget(message);
    layout->addWidget(button);

    setCentralWidget(central);

    connect(button, &QPushButton::clicked, message, [message]() {
        message->setText(
            QString::fromUtf8("Сигнал отримано — текст оновлено.")
        );
    });
}

} // namespace xray::app
