#include "app/main_window.hpp"
#include "ui/shared/icons.hpp"
#include "ui/shared/theme.hpp"

#include <QApplication>
#include <QDir>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QToolButton>

#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void checkWrappedLabels(QWidget& window) {
    for (auto* label : window.findChildren<QLabel*>()) {
        if (!label->isVisible() || !label->wordWrap()) continue;
        const int needed = label->heightForWidth(label->width());
        if (needed > label->height()) {
            std::cerr << label->text().toStdString() << ": height=" << label->height()
                      << ", required=" << needed << ", width=" << label->width() << '\n';
            throw std::runtime_error("Wrapped text is clipped");
        }
    }
}
} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setOrganizationName("CodeXrayGuiTests");
    app.setApplicationName("Design");
    app.setStyle("Fusion");
    const QString output = app.arguments().value(1);
    if (!output.isEmpty()) QDir().mkpath(output);
    QTemporaryDir settingsDirectory((output.isEmpty() ? QDir::tempPath() : output) + "/settings-XXXXXX");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());

    try {
        using namespace xray::ui;
        require(settingsDirectory.isValid(), "The settings test directory must be writable");
        require(loadTheme() == Theme::dark, "A fresh settings store must default to dark");
        applyTheme(app, Theme::dark);
        auto symbol = makeIcon(Symbol::appearance);
        const auto darkIcon = symbol.pixmap(QSize(24, 24)).toImage();
        const auto retina = symbol.pixmap(QSize(24, 24), 2.0);
        require(retina.devicePixelRatio() == 2 && retina.size() == QSize(48, 48),
                "Icons must render at the requested high-DPI scale");

        xray::app::MainWindow window(Theme::dark);
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.resize(1180, 760);
        window.show();
        app.processEvents();
        auto* pages = window.findChild<QListWidget*>("pageNavigation");
        auto* collapse = window.findChild<QToolButton*>("pagesToggle");
        auto* theme = window.findChild<QToolButton*>("themeToggle");
        auto* panel = window.findChild<QWidget*>("pagesPanel");
        auto* editor = window.findChild<QPlainTextEdit*>("sourceCode");
        auto* splitter = window.findChild<QSplitter*>("structureSplit");
        require(pages && collapse && theme && panel && editor && splitter, "GUI components must exist");
        require(pages->currentRow() == 2, "Structure must be the selected page");
        require(editor->isReadOnly(), "Source code must remain read-only");
        require(!(pages->item(0)->flags() & Qt::ItemIsEnabled), "Unimplemented pages must be disabled");
        checkWrappedLabels(window);
        if (!output.isEmpty()) require(window.grab().save(output + "/dark.png"), "Dark snapshot failed");

        collapse->click();
        app.processEvents();
        require(panel->isHidden(), "Collapsing navigation must hide its pages");
        theme->click();
        app.processEvents();
        require(panel->isHidden() && pages->currentRow() == 2,
                "Changing theme must retain collapsed navigation and page selection");
        require(window.findChild<QPlainTextEdit*>("sourceCode") == editor,
                "Changing theme must not recreate the source viewer");
        require(loadTheme() == Theme::light, "The selected theme must be saved");
        require(symbol.pixmap(QSize(24, 24)).toImage() != darkIcon,
                "Existing vector icons must update after changing theme");

        collapse->click();
        app.processEvents();
        checkWrappedLabels(window);
        if (!output.isEmpty()) require(window.grab().save(output + "/light.png"), "Light snapshot failed");
        splitter->setSizes({400, 600});
        app.processEvents();
        require(splitter->sizes().at(0) > 0 && splitter->sizes().at(1) > 0,
                "Both splitter panels must remain visible");
        window.resize(800, 540);
        app.processEvents();
        checkWrappedLabels(window);
        collapse->click();
        app.processEvents();
        if (!output.isEmpty()) require(window.grab().save(output + "/narrow.png"), "Narrow snapshot failed");
        window.close();

        auto font = app.font();
        font.setPointSizeF(font.pointSizeF() + 5);
        app.setFont(font);
        xray::app::MainWindow large(Theme::light);
        large.setAttribute(Qt::WA_DontShowOnScreen);
        large.resize(1180, 760);
        large.show();
        app.processEvents();
        checkWrappedLabels(large);
        if (!output.isEmpty()) require(large.grab().save(output + "/large-font.png"), "Large font snapshot failed");
        std::cout << "Themes, settings, navigation state, splitter, font wrapping and high-DPI icons: passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
