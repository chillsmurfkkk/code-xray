#include "main_window.hpp"
#include "ui/shared/theme.hpp"

#include "ui/code/structure_page.hpp"

#include <QApplication>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>
#include <QHBoxLayout>
#include <QSize>
#include <QString>
#include <QStyle>
#include <QToolButton>
#include <QListWidget>
#include <QListWidgetItem>
#include <QStringList>

namespace xray::app {

MainWindow::MainWindow(ui::Theme theme, QWidget* parent)
    : QMainWindow(parent),
    theme_(theme) {
    setWindowTitle("Code Xray");
    resize(960, 640);

    auto* central = new QWidget(this);
    auto* rootLayout = new QHBoxLayout(central);

    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(12);

    // Ліва панель навігації.
    auto* rail = new QWidget(central);
    rail->setFixedWidth(64);

    auto* railLayout = new QVBoxLayout(rail);
    railLayout->setContentsMargins(12, 12, 12, 12);
    railLayout->setSpacing(8);

    // Усі кнопки панелі створюємо за однаковими правилами.
    const auto addButton = [rail, railLayout](
        const QString& title,
        QStyle::StandardPixmap icon,
        bool available
    ) {
        auto* button = new QToolButton(rail);

        button->setText(title);
        button->setAccessibleName(title);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setIcon(rail->style()->standardIcon(icon));
        button->setIconSize(QSize(20, 20));
        button->setFixedSize(40, 40);
        button->setFocusPolicy(Qt::StrongFocus);
        button->setAutoRaise(true);
        button->setEnabled(available);

        button->setToolTip(
            available
                ? title
                : title + QString::fromUtf8(" — сторінки ще не підключені")
        );

        railLayout->addWidget(button);
        return button;
    };

    auto* codeButton = addButton(
        QString::fromUtf8("Код"),
        QStyle::SP_DirIcon,
        true
    );
    codeButton->setCheckable(true);
    codeButton->setAutoExclusive(true);
    codeButton->setChecked(true);

    addButton(
        QString::fromUtf8("Аналіз"),
        QStyle::SP_FileDialogDetailedView,
        false
    );
    addButton(
        QString::fromUtf8("Історія"),
        QStyle::SP_FileDialogBack,
        false
    );
    addButton(
        QString::fromUtf8("Порівняння"),
        QStyle::SP_FileDialogListView,
        false
    );

    auto* pagesToggle = addButton(
        QString::fromUtf8("Список сторінок"),
        QStyle::SP_ArrowLeft,
        true
    );
    pagesToggle->setCheckable(true);
    pagesToggle->setChecked(true);
    pagesToggle->setArrowType(Qt::LeftArrow);
    pagesToggle->setToolTip(QString::fromUtf8("Згорнути список сторінок"));

    // Увесь вільний простір залишається над перемикачем теми.
    railLayout->addStretch();

    auto* themeButton = addButton(
        QString::fromUtf8("Змінити тему"),
        QStyle::SP_DesktopIcon,
        true
    );

    auto* pagesPanel = new QWidget(central);
    pagesPanel->setFixedWidth(200);

    auto* pagesLayout = new QVBoxLayout(pagesPanel);
    pagesLayout->setContentsMargins(12, 12, 12, 12);
    pagesLayout->setSpacing(12);

    auto* sectionTitle = new QLabel(
        QString::fromUtf8("Код"),
        pagesPanel
    );

    auto* pagesList = new QListWidget(pagesPanel);
    pagesList->addItems(QStringList{
        QString::fromUtf8("Джерело"),
        QString::fromUtf8("Відбір файлів"),
        QString::fromUtf8("Структура"),
        QString::fromUtf8("Діагностика")
    });

    for (int row = 0; row < pagesList->count(); ++row) {
        auto* item = pagesList->item(row);
        item->setFlags(Qt::NoItemFlags);
        item->setToolTip(
            QString::fromUtf8("Сторінку ще не підключено")
        );
    }

    auto* structureItem = pagesList->item(2);
    structureItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    structureItem->setToolTip(
        QString::fromUtf8("Перегляд структури C++ коду")
    );

    pagesList->setCurrentRow(2);

    pagesLayout->addWidget(sectionTitle);
    pagesLayout->addWidget(pagesList, 1);

    // Поточна робоча область.
    auto* structurePage = new ui::StructurePage(central);

    rootLayout->addWidget(rail);
    rootLayout->addWidget(pagesPanel);
    rootLayout->addWidget(structurePage, 1);

    setCentralWidget(central);

    connect(themeButton, &QToolButton::clicked, this, [this]() {
        theme_ = theme_ == ui::Theme::dark
            ? ui::Theme::light
            : ui::Theme::dark;

        ui::applyTheme(*qApp, theme_);
        ui::saveTheme(theme_);
    });

    connect(
        pagesToggle,
        &QToolButton::toggled,
        pagesPanel,
        [pagesPanel, pagesToggle](bool expanded) {
            pagesPanel->setVisible(expanded);

            pagesToggle->setArrowType(
                expanded ? Qt::LeftArrow : Qt::RightArrow
            );

            pagesToggle->setToolTip(
                expanded
                    ? QString::fromUtf8("Згорнути список сторінок")
                    : QString::fromUtf8("Розгорнути список сторінок")
            );
        }
    );

}

} // namespace xray::app
