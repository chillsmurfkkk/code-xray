#include "main_window.hpp"

#include "ui/shared/theme.hpp"

#include <QApplication>
#include <QTimer>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);

    application.setOrganizationName("CodeXRay");
    application.setApplicationName("CodeXRay");
    application.setStyle("Fusion");

    const auto theme = xray::ui::loadTheme();
    xray::ui::applyTheme(application, theme);

    xray::app::MainWindow window(theme);
    window.show();

    if (application.arguments().contains("--smoke-test")) {
        QTimer::singleShot(100, &application, &QApplication::quit);
    }

    return application.exec();
}
