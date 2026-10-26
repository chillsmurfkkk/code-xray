#include "navigation.hpp"
#include "icons.hpp"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

namespace xray::ui {

NavigationPanel::NavigationPanel(QWidget* parent) : QFrame(parent) {
    setObjectName("navigationPanel");
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(0);

    rail_ = new QWidget(this);
    rail_->setObjectName("navigationRail");
    rail_->setFixedWidth(qMax(40, fontMetrics().height() + 16) + 24);
    auto* railLayout = new QVBoxLayout(rail_);
    railLayout->setContentsMargins(12, 16, 12, 12);
    railLayout->setSpacing(8);
    auto* brand = new QLabel("CX", rail_);
    brand->setObjectName("brandMark");
    brand->setAccessibleName("Code Xray");
    brand->setAlignment(Qt::AlignCenter);
    brand->setFixedSize(rail_->width() - 24, rail_->width() - 24);
    auto brandFont = brand->font();
    brandFont.setWeight(QFont::Bold);
    brand->setFont(brandFont);
    railLayout->addWidget(brand);
    railLayout->addSpacing(16);

    const auto addSection = [&](Symbol symbol, const QString& title, bool available) {
        auto* button = new IconButton(symbol, title, rail_);
        button->setEnabled(available);
        if (!available)
            button->setDescription(title + QString::fromUtf8(" — сторінки ще не підключені"));
        railLayout->addWidget(button);
        return button;
    };
    auto* code = addSection(Symbol::code, QString::fromUtf8("Код"), true);
    code->setObjectName("codeSection");
    code->setCheckable(true);
    code->setAutoExclusive(true);
    code->setChecked(true);
    addSection(Symbol::analysis, QString::fromUtf8("Аналіз"), false);
    addSection(Symbol::history, QString::fromUtf8("Історія"), false);
    addSection(Symbol::compare, QString::fromUtf8("Порівняння"), false);

    railLayout->addStretch();
    pagesToggle_ = new IconButton(Symbol::sidebarClose,
        QString::fromUtf8("Згорнути список сторінок"), rail_);
    pagesToggle_->setObjectName("pagesToggle");
    pagesToggle_->setCheckable(true);
    pagesToggle_->setChecked(true);
    railLayout->addWidget(pagesToggle_);
    themeButton_ = new IconButton(Symbol::appearance, QString::fromUtf8("Змінити тему"), rail_);
    themeButton_->setObjectName("themeToggle");
    railLayout->addWidget(themeButton_);

    divider_ = new QFrame(this);
    divider_->setObjectName("navigationDivider");
    divider_->setFixedWidth(1);
    pagesPanel_ = new QWidget(this);
    pagesPanel_->setObjectName("pagesPanel");
    pagesWidth_ = qMax(210, fontMetrics().horizontalAdvance(QString::fromUtf8("Відбір файлів")) + 88);
    pagesPanel_->setFixedWidth(pagesWidth_);
    auto* pagesLayout = new QVBoxLayout(pagesPanel_);
    pagesLayout->setContentsMargins(12, 20, 12, 12);
    pagesLayout->setSpacing(6);
    auto* title = new QLabel(QString::fromUtf8("Код"), pagesPanel_);
    title->setObjectName("sectionTitle");
    auto titleFont = title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() + 2);
    titleFont.setWeight(QFont::DemiBold);
    title->setFont(titleFont);
    auto* subtitle = new QLabel(QString::fromUtf8("Файли та структура"), pagesPanel_);
    subtitle->setObjectName("sectionCaption");
    pagesLayout->addWidget(title);
    pagesLayout->addWidget(subtitle);
    pagesLayout->addSpacing(14);

    auto* pages = new QListWidget(pagesPanel_);
    pages->setObjectName("pageNavigation");
    pages->setAccessibleName(QString::fromUtf8("Сторінки розділу Код"));
    pages->setIconSize(QSize(18, 18));
    pages->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    const auto addPage = [&](Symbol symbol, const QString& title, bool available) {
        auto* item = new QListWidgetItem(makeIcon(symbol), title, pages);
        item->setToolTip(available ? title
            : title + QString::fromUtf8(" — сторінку ще не підключено"));
        if (!available) item->setFlags(Qt::NoItemFlags);
    };
    addPage(Symbol::source, QString::fromUtf8("Джерело"), false);
    addPage(Symbol::filter, QString::fromUtf8("Відбір файлів"), false);
    addPage(Symbol::structure, QString::fromUtf8("Структура"), true);
    addPage(Symbol::diagnostics, QString::fromUtf8("Діагностика"), false);
    pages->setCurrentRow(2);
    pagesLayout->addWidget(pages, 1);
    layout->addWidget(rail_);
    layout->addWidget(divider_);
    layout->addWidget(pagesPanel_);
    connect(pagesToggle_, &QToolButton::toggled, this, &NavigationPanel::setExpanded);
    connect(themeButton_, &QToolButton::clicked, this, &NavigationPanel::themeToggleRequested);
    setExpanded(true);
}

void NavigationPanel::setExpanded(bool expanded) {
    pagesPanel_->setVisible(expanded);
    divider_->setVisible(expanded);
    pagesToggle_->setIcon(makeIcon(expanded ? Symbol::sidebarClose : Symbol::sidebarOpen));
    pagesToggle_->setDescription(expanded
        ? QString::fromUtf8("Згорнути список сторінок")
        : QString::fromUtf8("Розгорнути список сторінок"));
    setFixedWidth(rail_->width() + (expanded ? pagesWidth_ + 1 : 0) + 2);
}

void NavigationPanel::setTheme(Theme theme) {
    themeButton_->setDescription(theme == Theme::dark
        ? QString::fromUtf8("Увімкнути світлу тему")
        : QString::fromUtf8("Увімкнути темну тему"));
}
} // namespace xray::ui
