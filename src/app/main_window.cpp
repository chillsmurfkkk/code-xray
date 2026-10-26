#include "main_window.hpp"
#include "ui/code/structure_page.hpp"
#include "ui/shared/navigation.hpp"
#include "ui/shared/theme.hpp"

#include "ui/code/demo_snapshot.hpp"

#include <QApplication>
#include <QHBoxLayout>
#include <QScreen>

namespace xray::app {

MainWindow::MainWindow(ui::Theme theme, QWidget* parent)
    : QMainWindow(parent), theme_(theme) {
    setWindowTitle("Code Xray");
    setMinimumSize(800, 540);
    resize(QSize(1180, 760).boundedTo(screen()->availableGeometry().size() - QSize(48, 64)));

    auto* central = new QWidget(this);
    central->setObjectName("appRoot");
    auto* layout = new QHBoxLayout(central);
    layout->setContentsMargins(16, 16, 20, 16);
    layout->setSpacing(20);

    auto* navigation = new ui::NavigationPanel(central);
    navigation->setTheme(theme_);
    auto* structure = new ui::StructurePage(central);
    if (qApp->arguments().contains("--demo")) {
        structure->setSnapshot(ui::makeDemoSnapshot(), true);
    }
    layout->addWidget(navigation);
    layout->addWidget(structure, 1);
    setCentralWidget(central);

    connect(navigation, &ui::NavigationPanel::themeToggleRequested, this, [this, navigation]() {
        theme_ = theme_ == ui::Theme::dark ? ui::Theme::light : ui::Theme::dark;
        ui::applyTheme(*qApp, theme_);
        ui::saveTheme(theme_);
        navigation->setTheme(theme_);
    });
}

} // namespace xray::app
