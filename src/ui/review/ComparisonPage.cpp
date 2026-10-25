// ComparisonPage.cpp — D-UI1 «Порівняння»
#include "ui/review/ComparisonPage.hpp"
#include "ui/review/review_fixtures.hpp"
#include "review/api.hpp"

#include <QMessageBox>

namespace xray::ui::review {

using namespace xray::review;

ComparisonPage::ComparisonPage(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);

    // --- Заголовок ---
    auto* titleLabel = new QLabel(QString::fromUtf8("Порівняння"));
    auto titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    // --- Панель фільтрів ---
    auto* filterLayout = new QHBoxLayout;

    pathFilterEdit_ = new QLineEdit;
    pathFilterEdit_->setPlaceholderText(QString::fromUtf8("Фільтр за шляхом…"));
    pathFilterEdit_->setMinimumHeight(32);
    filterLayout->addWidget(pathFilterEdit_);

    kindFilterCombo_ = new QComboBox;
    kindFilterCombo_->setMinimumHeight(32);
    kindFilterCombo_->addItem(QString::fromUtf8("Усі види"), -1);
    kindFilterCombo_->addItem(QString::fromUtf8("Змінено"), static_cast<int>(ReviewKind::changed));
    kindFilterCombo_->addItem(QString::fromUtf8("Додано"), static_cast<int>(ReviewKind::added));
    kindFilterCombo_->addItem(QString::fromUtf8("Видалено"), static_cast<int>(ReviewKind::removed));
    kindFilterCombo_->addItem(QString::fromUtf8("Без змін"), static_cast<int>(ReviewKind::unchanged));
    kindFilterCombo_->addItem(QString::fromUtf8("Незіставлено"), static_cast<int>(ReviewKind::unmatched));
    filterLayout->addWidget(kindFilterCombo_);

    showUnknownCheck_ = new QCheckBox(QString::fromUtf8("Невідомі"));
    showUnknownCheck_->setChecked(true);
    filterLayout->addWidget(showUnknownCheck_);

    compareButton_ = new QPushButton(QString::fromUtf8("Порівняти"));
    compareButton_->setMinimumHeight(32);
    filterLayout->addWidget(compareButton_);

    mainLayout->addLayout(filterLayout);

    // --- Підсумок ---
    summaryLabel_ = new QLabel;
    mainLayout->addWidget(summaryLabel_);

    // --- Таблиця змін ---
    reviewTable_ = new QTableWidget;
    reviewTable_->setColumnCount(5);
    reviewTable_->setHorizontalHeaderLabels({
        QString::fromUtf8("Функція"),
        QString::fromUtf8("Вид"),
        QString::fromUtf8("Файл"),
        QString::fromUtf8("Достовірність"),
        QString::fromUtf8("Пояснення"),
    });
    reviewTable_->horizontalHeader()->setStretchLastSection(true);
    reviewTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    reviewTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    reviewTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    reviewTable_->verticalHeader()->setDefaultSectionSize(28);
    mainLayout->addWidget(reviewTable_, /*stretch=*/1);

    // --- Підключення сигналів ---
    connect(pathFilterEdit_, &QLineEdit::textChanged, this, &ComparisonPage::onReviewFilterChanged);
    connect(kindFilterCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ComparisonPage::onReviewFilterChanged);
    connect(showUnknownCheck_, &QCheckBox::stateChanged, this, &ComparisonPage::onReviewFilterChanged);
    connect(compareButton_, &QPushButton::clicked, this, &ComparisonPage::onCompareRequested);
    connect(reviewTable_, &QTableWidget::cellClicked, this, &ComparisonPage::onTableRowSelected);

    // Завантажити демо-дані
    setReport(makeDemoComparisonReport());
}

// D-H01: запуск порівняння
void ComparisonPage::onCompareRequested()
{
    // DEMO: перебудовуємо звіт із фікстурних даних.
    // У реальній версії тут буде виклик координатора C.
    setReport(makeDemoComparisonReport());
}

// D-H02: фільтрація таблиці
void ComparisonPage::onReviewFilterChanged()
{
    const auto pathText = pathFilterEdit_->text().toStdString();
    const int kindData = kindFilterCombo_->currentData().toInt();
    std::optional<ReviewKind> kindFilter;
    if (kindData >= 0) {
        kindFilter = static_cast<ReviewKind>(kindData);
    }
    const bool showUnknown = showUnknownCheck_->isChecked();

    auto filtered = filterReviewItems(currentReport_.items, /*textFilter=*/"",
                                      pathText, kindFilter, showUnknown);
    populateTable(filtered);
    updateSummary(filtered);
}

// D-H03: вибір рядка
void ComparisonPage::onTableRowSelected(int row, int /*column*/)
{
    if (row >= 0 && row < static_cast<int>(currentReport_.items.size())) {
        emit reviewItemSelected(row);
    }
}

void ComparisonPage::setReport(const ComparisonReport& report)
{
    currentReport_ = report;
    onReviewFilterChanged();
}

void ComparisonPage::populateTable(const std::vector<ReviewItem>& items)
{
    reviewTable_->setRowCount(static_cast<int>(items.size()));
    for (int row = 0; row < static_cast<int>(items.size()); ++row) {
        const auto& item = items[static_cast<std::size_t>(row)];
        reviewTable_->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(item.entityKey)));
        reviewTable_->setItem(row, 1, new QTableWidgetItem(kindToString(item.kind)));
        const auto path = item.sourceRefs.empty() ? "" : item.sourceRefs.front().relativePath;
        reviewTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(path)));
        reviewTable_->setItem(row, 3, new QTableWidgetItem(confidenceToString(item.confidence)));
        reviewTable_->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(item.explanation)));
    }
    reviewTable_->resizeColumnsToContents();
}

void ComparisonPage::updateSummary(const std::vector<ReviewItem>& items)
{
    auto summary = summarizeComparison(items);
    summaryLabel_->setText(QString::fromUtf8("Підсумок: ") + QString::fromStdString(summary));
}

QString ComparisonPage::kindToString(ReviewKind kind)
{
    switch (kind) {
    case ReviewKind::unchanged: return QString::fromUtf8("Без змін");
    case ReviewKind::changed:   return QString::fromUtf8("Змінено");
    case ReviewKind::added:     return QString::fromUtf8("Додано");
    case ReviewKind::removed:   return QString::fromUtf8("Видалено");
    case ReviewKind::unmatched: return QString::fromUtf8("Незіставлено");
    }
    return "?";
}

QString ComparisonPage::confidenceToString(Confidence conf)
{
    switch (conf) {
    case Confidence::exact:      return QString::fromUtf8("Точно");
    case Confidence::ambiguous:  return QString::fromUtf8("Неоднозначно");
    case Confidence::incomplete: return QString::fromUtf8("Неповно");
    }
    return "?";
}

} // namespace xray::ui::review
