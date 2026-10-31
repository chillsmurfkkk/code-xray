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

    codeButton_ = addSection(Symbol::code, QString::fromUtf8("Код"), true);
    codeButton_->setObjectName("codeSection");
    codeButton_->setCheckable(true);
    codeButton_->setAutoExclusive(true);
    codeButton_->setChecked(true);

    analysisButton_ = addSection(Symbol::analysis, QString::fromUtf8("Аналіз"), true);
    analysisButton_->setObjectName("analysisSection");
    analysisButton_->setCheckable(true);
    analysisButton_->setAutoExclusive(true);
    connect(analysisButton_, &QToolButton::clicked, this, [this] { showSection(Section::analysis); });

    historyButton_ = addSection(Symbol::history, QString::fromUtf8("Історія"), true);
    historyButton_->setObjectName("historySection");
    historyButton_->setCheckable(true);
    historyButton_->setAutoExclusive(true);
    connect(historyButton_, &QToolButton::clicked, this, [this] { showSection(Section::history); });

    compareButton_ = addSection(Symbol::compare, QString::fromUtf8("Порівняння"), true);
    compareButton_->setObjectName("compareSection");
    compareButton_->setCheckable(true);
    compareButton_->setAutoExclusive(true);

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

    sectionTitle_ = new QLabel(pagesPanel_);
    sectionTitle_->setObjectName("sectionTitle");
    auto titleFont = sectionTitle_->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() + 2);
    titleFont.setWeight(QFont::DemiBold);
    sectionTitle_->setFont(titleFont);

    sectionSubtitle_ = new QLabel(pagesPanel_);
    sectionSubtitle_->setObjectName("sectionCaption");

    pagesLayout->addWidget(sectionTitle_);
    pagesLayout->addWidget(sectionSubtitle_);
    pagesLayout->addSpacing(14);

    pagesList_ = new QListWidget(pagesPanel_);
    pagesList_->setObjectName("pageNavigation");
    pagesList_->setIconSize(QSize(18, 18));
    pagesList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pagesLayout->addWidget(pagesList_, 1);

    layout->addWidget(rail_);
    layout->addWidget(divider_);
    layout->addWidget(pagesPanel_);

    connect(pagesToggle_, &QToolButton::toggled, this, &NavigationPanel::setExpanded);
    connect(themeButton_, &QToolButton::clicked, this, &NavigationPanel::themeToggleRequested);

    connect(codeButton_, &QToolButton::clicked, this, [this]() {
        showSection(Section::code);
    });
    connect(compareButton_, &QToolButton::clicked, this, [this]() {
        showSection(Section::review);
    });

    connect(pagesList_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0) {
            emit pageSelected(currentSection_, row);
        }
    });

    showSection(Section::code);
    setExpanded(true);
}

void NavigationPanel::showSection(Section section) {
    currentSection_ = section;
    pagesList_->clear();

    const auto addPage = [this](Symbol symbol, const QString& title, bool available) {
        auto* item = new QListWidgetItem(makeIcon(symbol), title, pagesList_);
        item->setToolTip(available ? title
            : title + QString::fromUtf8(" — сторінку ще не підключено"));
        if (!available) item->setFlags(Qt::NoItemFlags);
    };

    if (section == Section::code) {
        sectionTitle_->setText(QString::fromUtf8("Код"));
        sectionSubtitle_->setText(QString::fromUtf8("Файли та структура"));
        addPage(Symbol::source, QString::fromUtf8("Джерело"), true);
        addPage(Symbol::filter, QString::fromUtf8("Відбір файлів"), true);
        addPage(Symbol::structure, QString::fromUtf8("Структура"), true);
        addPage(Symbol::diagnostics, QString::fromUtf8("Діагностика"), false);
        pagesList_->setCurrentRow(2);
    } else if (section == Section::analysis) {
        sectionTitle_->setText(QString::fromUtf8("Аналіз"));
        sectionSubtitle_->setText(QString::fromUtf8("Метрики та правила"));
        addPage(Symbol::document, QString::fromUtf8("Профілі"), true);
        addPage(Symbol::analysis, QString::fromUtf8("Метрики"), true);
        addPage(Symbol::filter, QString::fromUtf8("Правила"), true);
        addPage(Symbol::diagnostics, QString::fromUtf8("Порушення"), true);
        pagesList_->setCurrentRow(1);
    } else if (section == Section::history) {
        sectionTitle_->setText(QString::fromUtf8("Історія"));
        sectionSubtitle_->setText(QString::fromUtf8("Ревізії та аналіз змін"));
        addPage(Symbol::history, QString::fromUtf8("Журнал комітів"), true);
        addPage(Symbol::filter, QString::fromUtf8("Фільтри історії"), true);
        addPage(Symbol::analysis, QString::fromUtf8("Динаміка коду"), true);
        addPage(Symbol::structure, QString::fromUtf8("Дерево ревізій"), true);
        pagesList_->setCurrentRow(0);
    } else if (section == Section::review) {
        sectionTitle_->setText(QString::fromUtf8("Порівняння"));
        sectionSubtitle_->setText(QString::fromUtf8("Зміни, тренди та експорт"));
        addPage(Symbol::compare, QString::fromUtf8("Порівняння"), true);
        addPage(Symbol::diagnostics, QString::fromUtf8("Динаміка"), true);
        addPage(Symbol::structure, QString::fromUtf8("Картка зміни"), true);
        addPage(Symbol::document, QString::fromUtf8("Експорт"), true);
        pagesList_->setCurrentRow(0);
    }

    emit sectionChanged(section);
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

void NavigationPanel::setCurrentPage(Section section, int pageIndex) {
    if (currentSection_ != section) {
        showSection(section);
        if (section == Section::analysis && analysisButton_) {
            analysisButton_->setChecked(true);
        } else if (section == Section::history && historyButton_) {
            historyButton_->setChecked(true);
        } else if (section == Section::review && compareButton_) {
            compareButton_->setChecked(true);
        } else if (section == Section::code && codeButton_) {
            codeButton_->setChecked(true);
        }
    }
    if (pageIndex >= 0 && pageIndex < pagesList_->count()) {
        pagesList_->setCurrentRow(pageIndex);
    }
}

} // namespace xray::ui
