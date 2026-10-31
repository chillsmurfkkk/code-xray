#include "theme.hpp"

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QSettings>
#include <QScrollBar>
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

void styleWorkspaceScrollBar(QScrollBar* scrollBar) {
    scrollBar->setStyleSheet(QStringLiteral(R"QSS(
        QScrollBar:vertical { background: transparent; border: none; width: 8px; margin: 0; }
        QScrollBar::handle:vertical { background: palette(mid); min-height: 28px; border-radius: 4px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
    )QSS"));
}

SyntaxColors syntaxColors(const QPalette& palette) {
    const bool dark =
        palette.color(QPalette::Window).lightness() < 128;

    return {
        .keyword = QColor(dark ? "#fb4934" : "#9d0006"),
        .number = QColor(dark ? "#d3869b" : "#8f3f71"),
        .stringLiteral = QColor(dark ? "#b8bb26" : "#79740e"),
        .comment = palette.color(QPalette::PlaceholderText),
        .preprocessor = palette.color(QPalette::Accent)
    };
}

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

        QWidget#entityFilterPopup {
            background: transparent;
            border: none;
        }

        QComboBox[role="inputField"] {
            background: %1;
            color: %5;
            border: 1px solid %4;
            border-radius: 8px;
            padding: 6px 36px 6px 10px;
        }

        QComboBox[role="inputField"]:hover:enabled {
            background: %9;
        }

        QComboBox[role="inputField"]:focus,
        QComboBox[role="inputField"]:on {
            border-color: %7;
        }

        QComboBox[role="inputField"]:disabled {
            color: %11;
        }

        QComboBox[role="inputField"]::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 30px;

            background: transparent;
            border: none;
            border-top-right-radius: 8px;
            border-bottom-right-radius: 8px;
        }

        QComboBox[role="inputField"]::down-arrow { width: 10px; height: 6px; }
        QComboBox[role="inputField"] QAbstractItemView {
            background: %2; color: %5; border: 1px solid %4;
            border-radius: 8px; padding: 4px; outline: 0;

            selection-background-color: %3; selection-color: %5;
        }
        QComboBox[role="inputField"] QAbstractItemView::item {
            min-height: 24px; padding: 6px 10px;
            border: none; border-radius: 6px;
        }
        QComboBox[role="inputField"] QAbstractItemView::item:hover:!selected { background: %9; }
        QComboBox[role="inputField"] QAbstractItemView::item:selected { background: %3; color: %5; }

        QLineEdit[role="inputField"] {
            background: %1; color: %5; placeholder-text-color: %6;
            border: 1px solid %4; border-radius: 8px; padding: 6px 10px;
            selection-background-color: %3; selection-color: %5;
        }
        QLineEdit[role="inputField"]:hover:enabled { background: %9; }
        QLineEdit[role="inputField"]:focus { border-color: %7; }
        QLineEdit[role="inputField"]:disabled { color: %11; placeholder-text-color: %11; }

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
        QTreeWidget#structureTree, QListWidget#sourceFiles, QPlainTextEdit#sourceCode {
            background: transparent; color: %5;
            border: 1px solid transparent; border-radius: 8px; padding: 4px;
            selection-background-color: %3; selection-color: %5;
        }
        QTreeWidget#structureTree:focus, QPlainTextEdit#sourceCode:focus { border-color: %7; }
        QTreeWidget#structureTree { show-decoration-selected: 0; outline: 0; }
        QTreeWidget#structureTree::item { min-height: 20px; padding: 4px 8px; border: none; background: transparent; }
        QListWidget#sourceFiles:focus { border-color: %7; }
        QListWidget#sourceFiles::item { min-height: 20px; padding: 4px 8px; border: none; border-radius: 6px; }
        QListWidget#sourceFiles::item:selected { background: %3; color: %5; }
        QProgressBar#sourceProgress {
            background: %1; color: %5; border: 1px solid %4;
            border-radius: 6px; padding: 2px; min-height: 18px; text-align: center;
        }
        QProgressBar#sourceProgress::chunk { background: %7; border-radius: 4px; }
        QScrollArea#sourceFormScroll, QScrollArea#sourceFormScroll > QWidget#qt_scrollarea_viewport,
        QWidget#sourceFormContents { background: transparent; border: none; }
        QSplitter#structureSplit::handle { background: transparent; }
        QToolTip {
            background: %2; color: %5; border: 1px solid %4;
            border-radius: 8px; padding: 7px 10px;
        }
        QWidget#pagesPanel QScrollBar:vertical,
        QScrollBar#structureScrollBar:vertical,
        QScrollBar#sourceFormScrollBar:vertical,
        QPlainTextEdit#sourceCode QScrollBar:vertical,
        QTableWidget QScrollBar:vertical,
        QTableView QScrollBar:vertical {
            width: 8px; background: transparent; margin: 0;
        }
        QWidget#pagesPanel QScrollBar::handle:vertical,
        QScrollBar#structureScrollBar::handle:vertical,
        QScrollBar#sourceFormScrollBar::handle:vertical,
        QPlainTextEdit#sourceCode QScrollBar::handle:vertical,
        QTableWidget QScrollBar::handle:vertical,
        QTableView QScrollBar::handle:vertical {
            background: %4; min-height: 28px; border-radius: 4px;
        }
        QWidget#pagesPanel QScrollBar::add-line:vertical,
        QWidget#pagesPanel QScrollBar::sub-line:vertical,
        QScrollBar#structureScrollBar::add-line:vertical,
        QScrollBar#structureScrollBar::sub-line:vertical,
        QScrollBar#sourceFormScrollBar::add-line:vertical,
        QScrollBar#sourceFormScrollBar::sub-line:vertical,
        QPlainTextEdit#sourceCode QScrollBar::add-line:vertical,
        QPlainTextEdit#sourceCode QScrollBar::sub-line:vertical,
        QTableWidget QScrollBar::add-line:vertical,
        QTableWidget QScrollBar::sub-line:vertical,
        QTableView QScrollBar::add-line:vertical,
        QTableView QScrollBar::sub-line:vertical { height: 0; }
        QWidget#pagesPanel QScrollBar::add-page:vertical,
        QWidget#pagesPanel QScrollBar::sub-page:vertical,
        QScrollBar#structureScrollBar::add-page:vertical,
        QScrollBar#structureScrollBar::sub-page:vertical,
        QScrollBar#sourceFormScrollBar::add-page:vertical,
        QScrollBar#sourceFormScrollBar::sub-page:vertical,
        QPlainTextEdit#sourceCode QScrollBar::add-page:vertical,
        QPlainTextEdit#sourceCode QScrollBar::sub-page:vertical,
        QTableWidget QScrollBar::add-page:vertical,
        QTableWidget QScrollBar::sub-page:vertical,
        QTableView QScrollBar::add-page:vertical,
        QTableView QScrollBar::sub-page:vertical { background: none; }

        QTableWidget, QTableView {
            background-color: %2;
            color: %5;
            border: 1px solid %4;
            border-radius: 12px;
            gridline-color: transparent;
            outline: 0;
            padding: 2px;
        }
        QTableWidget:focus, QTableView:focus {
            border-color: %7;
        }
        QHeaderView {
            background-color: transparent;
            border: none;
        }
        QHeaderView::section {
            background-color: %2;
            color: %6;
            padding: 8px 12px;
            border: none;
            border-bottom: 1px solid %4;
            font-weight: 600;
        }
        QHeaderView::section:horizontal:first {
            border-top-left-radius: 10px;
        }
        QHeaderView::section:horizontal:last {
            border-top-right-radius: 10px;
        }
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
