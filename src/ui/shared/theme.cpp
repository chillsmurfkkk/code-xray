#include "theme.hpp"

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QSettings>
#include <QString>

namespace xray::ui {
namespace {
QColor blend(const QColor& from, const QColor& to, qreal amount) {
    return QColor::fromRgbF(
        from.redF() + (to.redF() - from.redF()) * amount,
        from.greenF() + (to.greenF() - from.greenF()) * amount,
        from.blueF() + (to.blueF() - from.blueF()) * amount);
}
} // namespace

void applyTheme(QApplication& application, Theme theme) {
    const bool dark = theme == Theme::dark;
    const QColor background(dark ? "#282828" : "#fbf1c7");
    const QColor surface(dark ? "#32302f" : "#f9f5d7");
    const QColor selection(dark ? "#3c3836" : "#ebdbb2");
    const QColor border(dark ? "#504945" : "#d5c4a1");
    const QColor text(dark ? "#ebdbb2" : "#3c3836");
    const QColor secondaryText(dark ? "#bdae93" : "#665c54");
    const QColor accent(dark ? "#8ec07c" : "#427b58");
    const QColor seam = blend(surface, border, 0.4);
    const QColor hover = blend(surface, selection, 0.6);
    const QColor accentSurface = blend(surface, accent, dark ? 0.16 : 0.11);
    const QColor disabledText = blend(surface, secondaryText, 0.65);

    QPalette palette = application.palette();
    palette.setColor(QPalette::Window, background);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, surface);
    palette.setColor(QPalette::AlternateBase, hover);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::PlaceholderText, secondaryText);
    palette.setColor(QPalette::Button, surface);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::ToolTipBase, surface);
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::Highlight, selection);
    palette.setColor(QPalette::HighlightedText, text);
    palette.setColor(QPalette::Accent, accent);
    palette.setColor(QPalette::Link, accent);
    palette.setColor(QPalette::Mid, seam);
    for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        palette.setColor(QPalette::Disabled, role, disabledText);
    application.setPalette(palette);

    // Opaque surfaces: depth comes from tone and a slight highlight, without blur.
    application.setStyleSheet(QStringLiteral(R"QSS(
        QWidget#appRoot { background: %1; }
        QFrame#navigationPanel, QFrame#workPanel {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 %8, stop:0.35 %2, stop:1 %2);
            border: 1px solid %4;
            border-radius: 16px;
        }
        QWidget#navigationRail, QWidget#pagesPanel { background: transparent; }
        QFrame#navigationDivider { background: %4; border: none; margin: 20px 0px; }
        QLabel#brandMark {
            color: %7; background: %10; border-radius: 12px;
        }
        QWidget#navigationRail QToolButton {
            background: transparent; border: 2px solid transparent;
            border-radius: 12px; padding: 0px;
        }
        QWidget#navigationRail QToolButton:hover:enabled { background: %9; }
        QWidget#navigationRail QToolButton#codeSection:checked { background: %10; }
        QWidget#navigationRail QToolButton:focus,
        QWidget#navigationRail QToolButton#codeSection:focus { border-color: %7; }
        QListWidget#pageNavigation {
            background: transparent; border: 1px solid transparent;
            border-radius: 12px; outline: 0; padding: 2px;
        }
        QListWidget#pageNavigation:focus { border-color: %7; }
        QListWidget#pageNavigation::item {
            color: %5; border: none; border-radius: 10px;
            padding: 10px 8px; margin: 2px 0px;
        }
        QListWidget#pageNavigation::item:hover:enabled:!selected { background: %9; }
        QListWidget#pageNavigation::item:selected { background: %3; color: %5; }
        QListWidget#pageNavigation::item:disabled { color: %11; }
        QLabel#sectionTitle, QLabel#panelTitle, QLabel#emptyTitle { color: %5; font-weight: 600; }

        QPushButton[role="primaryAction"] {
            background: %10; color: %5; border: 2px solid %4;
            border-radius: 10px; padding: 6px 14px; font-weight: 600;
        }
        QPushButton[role="primaryAction"]:hover:enabled { background: %9; border-color: %7; }
        QPushButton[role="primaryAction"]:pressed:enabled { background: %3; border-color: %7; }
        QPushButton[role="primaryAction"]:focus { border-color: %7; }
        QPushButton[role="primaryAction"]:disabled { background: %2; color: %11; border-color: %4; }

        QLineEdit#sourcePath {
            background: transparent; color: %5; border: 1px solid transparent;
            border-radius: 6px; padding: 4px; font-weight: 600;
        }
        QLineEdit#sourcePath:focus { border-color: %7; }
        QLabel#sectionCaption, QLabel#sourceContext,
        QLabel#emptyDescription, QLabel#panelCaption { color: %6; }
        QLabel#statusPill, QLabel#countPill {
            background: %9; color: %6; border: none;
            border-radius: 10px; padding: 5px 10px;
        }
        QTreeWidget#structureTree, QPlainTextEdit#sourceCode {
            background: transparent; color: %5;
            border: 1px solid transparent; border-radius: 8px; padding: 4px;
            selection-background-color: %3; selection-color: %5;
        }
        QTreeWidget#structureTree:focus, QPlainTextEdit#sourceCode:focus { border-color: %7; }
        QTreeWidget#structureTree { show-decoration-selected: 0; outline: 0; }
        QTreeWidget#structureTree::item { min-height: 20px; padding: 4px 8px; border: none; background: transparent; }
        QSplitter#structureSplit::handle { background: transparent; }
        QToolTip {
            background: %2; color: %5; border: 1px solid %4;
            border-radius: 8px; padding: 7px 10px;
        }
        QWidget#pagesPanel QScrollBar:vertical,
        QTreeWidget#structureTree QScrollBar:vertical,
        QPlainTextEdit#sourceCode QScrollBar:vertical {
            width: 8px; background: transparent; margin: 0;
        }
        QWidget#pagesPanel QScrollBar::handle:vertical,
        QTreeWidget#structureTree QScrollBar::handle:vertical,
        QPlainTextEdit#sourceCode QScrollBar::handle:vertical {
            background: %4; min-height: 28px; border-radius: 4px;
        }
        QWidget#pagesPanel QScrollBar::add-line:vertical,
        QWidget#pagesPanel QScrollBar::sub-line:vertical,
        QTreeWidget#structureTree QScrollBar::add-line:vertical,
        QTreeWidget#structureTree QScrollBar::sub-line:vertical,
        QPlainTextEdit#sourceCode QScrollBar::add-line:vertical,
        QPlainTextEdit#sourceCode QScrollBar::sub-line:vertical { height: 0; }
        QWidget#pagesPanel QScrollBar::add-page:vertical,
        QWidget#pagesPanel QScrollBar::sub-page:vertical,
        QTreeWidget#structureTree QScrollBar::add-page:vertical,
        QTreeWidget#structureTree QScrollBar::sub-page:vertical,
        QPlainTextEdit#sourceCode QScrollBar::add-page:vertical,
        QPlainTextEdit#sourceCode QScrollBar::sub-page:vertical { background: none; }
    )QSS")
        .arg(background.name(), surface.name(), selection.name(), seam.name(), text.name(),
             secondaryText.name(), accent.name(), surface.lighter(dark ? 104 : 101).name(),
             hover.name(), accentSurface.name(), disabledText.name()));
}

Theme loadTheme() {
    QSettings settings;
    return settings.value("ui/theme", QStringLiteral("dark")).toString() == QStringLiteral("light")
        ? Theme::light : Theme::dark;
}

void saveTheme(Theme theme) {
    QSettings settings;
    settings.setValue("ui/theme", theme == Theme::dark ? QStringLiteral("dark") : QStringLiteral("light"));
}

} // namespace xray::ui
