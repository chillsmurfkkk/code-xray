#include "main_window.hpp"
#include "windows_uia_filter.hpp"

#include "ui/shared/theme.hpp"
#include "ui/shared/popup_style.hpp"

#include <QApplication>
#include <QTimer>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);

#ifdef Q_OS_WIN
    // The filter must outlive the window and QApplication's event loop.
    xray::app::WindowsUiaFilter uiaFilter;
    if (application.arguments().contains("--no-uia")) {
        application.installNativeEventFilter(&uiaFilter);
    }
#endif

    application.setOrganizationName("CodeXRay");
    application.setApplicationName("CodeXRay");
    xray::ui::installPopupStyle(application);

    const auto theme = xray::ui::loadTheme();
    xray::ui::applyTheme(application, theme);

    xray::app::MainWindow window(theme);
    window.show();

    if (application.arguments().contains("--smoke-test")) {
        QTimer::singleShot(100, &application, &QApplication::quit);
    }

    return application.exec();
}
