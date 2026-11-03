#include "ui/history/HistoryFiltersPage.hpp"
#include "ui/history/history_fixtures.hpp"
#include "ui/review/review_table_delegate.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QDateTime>
#include <QEvent>

namespace xray::ui::history {

HistoryFiltersPage::HistoryFiltersPage(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // --- Header ---
    auto* headerLayout = new QHBoxLayout;
    auto* titleLabel = new QLabel(QString::fromUtf8("Фільтри історії"));
    auto titleFont = titleLabel->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() + 4);
    titleFont.setWeight(QFont::Bold);
    titleLabel->setFont(titleFont);
    headerLayout->addWidget(titleLabel);

    demoBadge_ = new QLabel(QString::fromUtf8("Демо-режим"), this);
    demoBadge_->setObjectName("filterDemoBadge");
    demoBadge_->setStyleSheet(
        "background: #d65d0e; color: #fbf1c7; font-weight: bold; "
        "border-radius: 8px; padding: 3px 8px; font-size: 11px;"
    );
    demoBadge_->setToolTip(QString::fromUtf8("Відображаються демонстраційні фікстури ревізій."));
    headerLayout->addWidget(demoBadge_);

    activeFiltersBadge_ = new QLabel(QString::fromUtf8("Фільтри не активні"), this);
    activeFiltersBadge_->setObjectName("activeFiltersBadge");
    activeFiltersBadge_->setStyleSheet(
        "background: palette(mid); color: palette(window-text); font-weight: bold; "
        "border-radius: 8px; padding: 3px 8px; font-size: 11px;"
    );
    headerLayout->addWidget(activeFiltersBadge_);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    // --- Filters GroupBox ---
    auto* filterBox = new QGroupBox(QString::fromUtf8("Параметри запитів (HistoryQuery)"), this);
    filterBox->setObjectName("filterBox");
    auto* gridLayout = new QGridLayout(filterBox);
    gridLayout->setContentsMargins(12, 12, 12, 12);
    gridLayout->setSpacing(8);

    // 1. CommitRangeQuery
    gridLayout->addWidget(new QLabel(QString::fromUtf8("Діапазон OID (початок..кінець):")), 0, 0);
    auto* rangeLayout = new QHBoxLayout;
    rangeStartEdit_ = new QLineEdit(this);
    rangeStartEdit_->setObjectName("filterRangeStartEdit");
    rangeStartEdit_->setPlaceholderText(QString::fromUtf8("Стартовий OID (або порожній)"));
    rangeStartEdit_->setProperty("role", "inputField");
    rangeEndEdit_ = new QLineEdit(this);
    rangeEndEdit_->setObjectName("filterRangeEndEdit");
    rangeEndEdit_->setPlaceholderText(QString::fromUtf8("Кінцевий OID (або порожній)"));
    rangeEndEdit_->setProperty("role", "inputField");
    rangeLayout->addWidget(rangeStartEdit_);
    rangeLayout->addWidget(new QLabel(".."));
    rangeLayout->addWidget(rangeEndEdit_);
    gridLayout->addLayout(rangeLayout, 0, 1, 1, 2);

    // 2. AuthorHistoryQuery
    gridLayout->addWidget(new QLabel(QString::fromUtf8("Автор / Email:")), 1, 0);
    authorEdit_ = new QLineEdit(this);
    authorEdit_->setObjectName("filterAuthorEdit");
    authorEdit_->setPlaceholderText(QString::fromUtf8("Ім'я або email автора"));
    authorEdit_->setProperty("role", "inputField");
    authorCaseCheck_ = new QCheckBox(QString::fromUtf8("Враховувати регістр автора"), this);
    authorCaseCheck_->setObjectName("filterAuthorCaseCheck");
    gridLayout->addWidget(authorEdit_, 1, 1);
    gridLayout->addWidget(authorCaseCheck_, 1, 2);

    // 3. MessageKeywordQuery
    gridLayout->addWidget(new QLabel(QString::fromUtf8("Ключове слово в описі:")), 2, 0);
    keywordEdit_ = new QLineEdit(this);
    keywordEdit_->setObjectName("filterKeywordEdit");
    keywordEdit_->setPlaceholderText(QString::fromUtf8("Слово в повідомленні"));
    keywordEdit_->setProperty("role", "inputField");
    auto* kwOptionsLayout = new QHBoxLayout;
    keywordCaseCheck_ = new QCheckBox(QString::fromUtf8("Регістр"), this);
    keywordCaseCheck_->setObjectName("filterKeywordCaseCheck");
    minLengthSpin_ = new QSpinBox(this);
    minLengthSpin_->setObjectName("filterMinLengthSpin");
    minLengthSpin_->setRange(0, 100);
    minLengthSpin_->setValue(0);
    minLengthSpin_->setPrefix(QString::fromUtf8("Мін. довжина: "));
    kwOptionsLayout->addWidget(keywordCaseCheck_);
    kwOptionsLayout->addWidget(minLengthSpin_);
    gridLayout->addWidget(keywordEdit_, 2, 1);
    gridLayout->addLayout(kwOptionsLayout, 2, 2);

    // 4. MergeCommitQuery
    gridLayout->addWidget(new QLabel(QString::fromUtf8("Коміти злиття (Merge):")), 3, 0);
    auto* mergeLayout = new QHBoxLayout;
    onlyMergesCheck_ = new QCheckBox(QString::fromUtf8("Тільки merge-коміти"), this);
    onlyMergesCheck_->setObjectName("filterOnlyMergesCheck");
    minParentsSpin_ = new QSpinBox(this);
    minParentsSpin_->setObjectName("filterMinParentsSpin");
    minParentsSpin_->setRange(2, 8);
    minParentsSpin_->setValue(2);
    minParentsSpin_->setPrefix(QString::fromUtf8("Мін. батьків: "));
    mergeLayout->addWidget(onlyMergesCheck_);
    mergeLayout->addWidget(minParentsSpin_);
    gridLayout->addLayout(mergeLayout, 3, 1, 1, 2);

    // Action buttons
    auto* btnLayout = new QHBoxLayout;
    applyBtn_ = new QPushButton(QString::fromUtf8("Застосувати фільтри"), this);
    applyBtn_->setObjectName("filterApplyBtn");
    applyBtn_->setMinimumHeight(32);
    applyBtn_->setProperty("role", "primaryAction");

    resetBtn_ = new QPushButton(QString::fromUtf8("Скинути фільтри"), this);
    resetBtn_->setObjectName("filterResetBtn");
    resetBtn_->setMinimumHeight(32);

    btnLayout->addWidget(applyBtn_);
    btnLayout->addWidget(resetBtn_);
    btnLayout->addStretch();

    gridLayout->addLayout(btnLayout, 4, 1, 1, 2);
    mainLayout->addWidget(filterBox);

    // Status label
    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName("filterStatusLabel");
    statusLabel_->setStyleSheet("color: palette(window-text); padding: 4px;");
    mainLayout->addWidget(statusLabel_);

    // Results table
    resultsTable_ = new QTableWidget(this);
    resultsTable_->setObjectName("filterResultsTable");
    resultsTable_->setColumnCount(5);
    resultsTable_->setHorizontalHeaderLabels({
        QString::fromUtf8("OID"),
        QString::fromUtf8("Автор"),
        QString::fromUtf8("Дата"),
        QString::fromUtf8("Батьків OID"),
        QString::fromUtf8("Повідомлення")
    });
    xray::ui::review::setupReviewTable(resultsTable_);
    resultsTable_->horizontalHeader()->setStretchLastSection(true);
    resultsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    resultsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    resultsTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    resultsTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    mainLayout->addWidget(resultsTable_, 1);

    connect(applyBtn_, &QPushButton::clicked, this, &HistoryFiltersPage::onApplyFilters);
    connect(resetBtn_, &QPushButton::clicked, this, &HistoryFiltersPage::onResetFilters);
    connect(resultsTable_, &QTableWidget::cellClicked, this, &HistoryFiltersPage::onTableRowSelected);

    setCommits(makeDemoCommits(), true);
}

void HistoryFiltersPage::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) {
        updateBadge(activeFilterCount_);
        refreshStatusStyle();
    }
}

void HistoryFiltersPage::setCommits(const std::vector<xray::history::CommitRecord>& commits, bool isDemo) {
    rawCommits_ = commits;
    isDemo_ = isDemo;
    if (demoBadge_) {
        demoBadge_->setVisible(isDemo_);
    }
    onResetFilters();
}

void HistoryFiltersPage::updateBadge(int activeCount) {
    activeFilterCount_ = activeCount;
    const bool dark = palette().color(QPalette::Window).lightness() < 128;
    if (activeCount > 0) {
        activeFiltersBadge_->setText(QString::fromUtf8("Активно фільтрів: %1").arg(activeCount));
        const QString bg = dark ? "#b8bb26" : "#427b58";
        const QString fg = dark ? "#282828" : "#fbf1c7";
        activeFiltersBadge_->setStyleSheet(QString(
            "background: %1; color: %2; font-weight: bold; border-radius: 8px; padding: 3px 8px; font-size: 11px;"
        ).arg(bg, fg));
    } else {
        activeFiltersBadge_->setText(QString::fromUtf8("Фільтри не активні"));
        activeFiltersBadge_->setStyleSheet(
            "background: palette(mid); color: palette(window-text); font-weight: bold; border-radius: 8px; padding: 3px 8px; font-size: 11px;"
        );
    }
}

void HistoryFiltersPage::refreshStatusStyle() {
    const bool dark = palette().color(QPalette::Window).lightness() < 128;
    if (statusKind_ == StatusKind::Success) {
        const QString successColor = dark ? "#b8bb26" : "#427b58";
        statusLabel_->setStyleSheet(QString("color: %1; font-weight: bold; padding: 4px;").arg(successColor));
    } else if (statusKind_ == StatusKind::Error) {
        const QString errColor = dark ? "#fb4934" : "#9d0006";
        statusLabel_->setStyleSheet(QString("color: %1; font-weight: bold; padding: 4px;").arg(errColor));
    } else {
        statusLabel_->setStyleSheet("color: palette(window-text); padding: 4px;");
    }
}

void HistoryFiltersPage::onApplyFilters() {
    std::vector<xray::history::CommitRecord> current = rawCommits_;
    int activeCount = 0;

    try {
        // Range filter
        QString start = rangeStartEdit_->text().trimmed();
        QString end = rangeEndEdit_->text().trimmed();
        if (!start.isEmpty() || !end.isEmpty()) {
            xray::history::CommitRangeQuery rangeQuery(start.toStdString(), end.toStdString());
            current = rangeQuery.apply(current);
            activeCount++;
        }

        // Author filter
        QString author = authorEdit_->text().trimmed();
        if (!author.isEmpty()) {
            xray::history::AuthorHistoryQuery authorQuery(author.toStdString(), authorCaseCheck_->isChecked());
            current = authorQuery.apply(current);
            activeCount++;
        }

        // Keyword filter
        QString keyword = keywordEdit_->text().trimmed();
        if (!keyword.isEmpty()) {
            xray::history::MessageKeywordQuery kwQuery(keyword.toStdString(),
                keywordCaseCheck_->isChecked(),
                static_cast<std::size_t>(minLengthSpin_->value()));
            current = kwQuery.apply(current);
            activeCount++;
        }

        // Merge filter
        if (onlyMergesCheck_->isChecked()) {
            xray::history::MergeCommitQuery mergeQuery(true, static_cast<std::size_t>(minParentsSpin_->value()));
            current = mergeQuery.apply(current);
            activeCount++;
        }

        statusKind_ = StatusKind::Success;
        refreshStatusStyle();
        statusLabel_->setText(QString::fromUtf8("Успішно відфільтровано: знайдено %1 з %2 комітів")
            .arg(current.size())
            .arg(rawCommits_.size()));

        updateBadge(activeCount);
        populateTable(current);

    } catch (const std::exception& e) {
        statusKind_ = StatusKind::Error;
        refreshStatusStyle();
        statusLabel_->setText(QString::fromUtf8("Помилка виконання фільтра: %1").arg(QString::fromUtf8(e.what()).toHtmlEscaped()));
        populateTable({});
    }
}

void HistoryFiltersPage::onResetFilters() {
    rangeStartEdit_->clear();
    rangeEndEdit_->clear();
    authorEdit_->clear();
    authorCaseCheck_->setChecked(false);
    keywordEdit_->clear();
    keywordCaseCheck_->setChecked(false);
    minLengthSpin_->setValue(0);
    onlyMergesCheck_->setChecked(false);
    minParentsSpin_->setValue(2);

    updateBadge(0);

    statusKind_ = StatusKind::Neutral;
    refreshStatusStyle();
    if (rawCommits_.empty()) {
        statusLabel_->setText(QString::fromUtf8("Історія порожня. Немає комітів для відображення."));
    } else {
        statusLabel_->setText(QString::fromUtf8("Фільтри скинуто. Відображаються всі %1 комітів").arg(rawCommits_.size()));
    }

    populateTable(rawCommits_);
}

void HistoryFiltersPage::onTableRowSelected(int, int) {
}

void HistoryFiltersPage::populateTable(const std::vector<xray::history::CommitRecord>& list) {
    filteredCommits_ = list;
    resultsTable_->setRowCount(static_cast<int>(list.size()));

    for (int r = 0; r < static_cast<int>(list.size()); ++r) {
        const auto& c = list[static_cast<std::size_t>(r)];

        auto* itemOid = new QTableWidgetItem(QString::fromStdString(c.oid));
        itemOid->setFont(QFont("monospace"));
        auto* itemAuthor = new QTableWidgetItem(QString::fromStdString(c.authorName));

        auto sec = c.authoredAt.time_since_epoch().count();
        QDateTime dt = QDateTime::fromSecsSinceEpoch(sec);
        auto* itemDate = new QTableWidgetItem(dt.toString("yyyy-MM-dd hh:mm"));

        auto* itemParents = new QTableWidgetItem(QString::number(c.parentOids.size()));
        auto* itemMsg = new QTableWidgetItem(QString::fromStdString(c.message));

        resultsTable_->setItem(r, 0, itemOid);
        resultsTable_->setItem(r, 1, itemAuthor);
        resultsTable_->setItem(r, 2, itemDate);
        resultsTable_->setItem(r, 3, itemParents);
        resultsTable_->setItem(r, 4, itemMsg);
    }
}

} // namespace xray::ui::history
