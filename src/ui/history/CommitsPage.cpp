#include "ui/history/CommitsPage.hpp"
#include "ui/history/history_fixtures.hpp"
#include "ui/review/review_table_delegate.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QDateTime>

namespace xray::ui::history {

CommitsPage::CommitsPage(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // --- Header ---
    auto* headerLayout = new QHBoxLayout;
    auto* titleLabel = new QLabel(QString::fromUtf8("Журнал комітів"));
    auto titleFont = titleLabel->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() + 4);
    titleFont.setWeight(QFont::Bold);
    titleLabel->setFont(titleFont);
    headerLayout->addWidget(titleLabel);

    demoBadge_ = new QLabel(QString::fromUtf8("Демо-режим"), this);
    demoBadge_->setObjectName("commitsDemoBadge");
    demoBadge_->setStyleSheet(
        "background: #d65d0e; color: #fbf1c7; font-weight: bold; "
        "border-radius: 8px; padding: 3px 8px; font-size: 11px;"
    );
    demoBadge_->setToolTip(QString::fromUtf8("Відображаються демонстраційні фікстури ревізій."));
    headerLayout->addWidget(demoBadge_);
    headerLayout->addStretch();

    countLabel_ = new QLabel(this);
    countLabel_->setObjectName("panelCaption");
    headerLayout->addWidget(countLabel_);

    mainLayout->addLayout(headerLayout);

    // --- Controls Bar ---
    auto* controlsLayout = new QHBoxLayout;
    controlsLayout->setSpacing(8);

    searchEdit_ = new QLineEdit(this);
    searchEdit_->setObjectName("commitSearchEdit");
    searchEdit_->setPlaceholderText(QString::fromUtf8("Пошук за автором, OID або повідомленням…"));
    searchEdit_->setMinimumHeight(32);
    searchEdit_->setProperty("role", "inputField");
    controlsLayout->addWidget(searchEdit_, 2);

    selectionCombo_ = new QComboBox(this);
    selectionCombo_->setObjectName("commitSelectionCombo");
    selectionCombo_->setMinimumHeight(32);
    selectionCombo_->setProperty("role", "inputField");
    selectionCombo_->addItem(QString::fromUtf8("Усі коміти"), 0);
    selectionCombo_->addItem(QString::fromUtf8("Останні N комітів"), 1);
    selectionCombo_->addItem(QString::fromUtf8("Періодична вибірка (крок 2)"), 2);
    controlsLayout->addWidget(selectionCombo_, 1);

    limitSpin_ = new QSpinBox(this);
    limitSpin_->setObjectName("commitLimitSpin");
    limitSpin_->setMinimumHeight(32);
    limitSpin_->setProperty("role", "inputField");
    limitSpin_->setRange(1, 100);
    limitSpin_->setValue(20);
    limitSpin_->setPrefix(QString::fromUtf8("Ліміт: "));
    controlsLayout->addWidget(limitSpin_);

    sortBtn_ = new QPushButton(QString::fromUtf8("Сортування: найновіші"), this);
    sortBtn_->setObjectName("commitSortBtn");
    sortBtn_->setMinimumHeight(32);
    controlsLayout->addWidget(sortBtn_);

    refreshBtn_ = new QPushButton(QString::fromUtf8("Оновити"), this);
    refreshBtn_->setObjectName("commitRefreshBtn");
    refreshBtn_->setMinimumHeight(32);
    controlsLayout->addWidget(refreshBtn_);

    mainLayout->addLayout(controlsLayout);

    // --- Table ---
    commitsTable_ = new QTableWidget(this);
    commitsTable_->setObjectName("commitsTable");
    commitsTable_->setColumnCount(6);
    commitsTable_->setHorizontalHeaderLabels({
        QString::fromUtf8("OID"),
        QString::fromUtf8("Автор"),
        QString::fromUtf8("Email"),
        QString::fromUtf8("Дата"),
        QString::fromUtf8("Батьки"),
        QString::fromUtf8("Повідомлення")
    });
    xray::ui::review::setupReviewTable(commitsTable_);
    commitsTable_->horizontalHeader()->setStretchLastSection(true);
    commitsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    commitsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    commitsTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    commitsTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    commitsTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    mainLayout->addWidget(commitsTable_, 1);

    // --- Details panel ---
    detailsLabel_ = new QLabel(QString::fromUtf8("Виберіть коміт для перегляду деталей"), this);
    detailsLabel_->setObjectName("commitDetailsLabel");
    detailsLabel_->setStyleSheet("background: palette(base); color: palette(text); border: 1px solid palette(mid); border-radius: 8px; padding: 10px; font-family: monospace;");
    detailsLabel_->setWordWrap(true);
    mainLayout->addWidget(detailsLabel_);

    // Connections
    connect(searchEdit_, &QLineEdit::textChanged, this, &CommitsPage::onSearchChanged);
    connect(selectionCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CommitsPage::onSelectionChanged);
    connect(limitSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &CommitsPage::onSelectionChanged);
    connect(sortBtn_, &QPushButton::clicked, this, &CommitsPage::onSortToggled);
    connect(refreshBtn_, &QPushButton::clicked, this, &CommitsPage::onRefreshClicked);
    connect(commitsTable_, &QTableWidget::cellClicked, this, &CommitsPage::onTableRowSelected);
    connect(commitsTable_, &QTableWidget::currentItemChanged, this, [this](QTableWidgetItem* current, QTableWidgetItem*) {
        if (current) {
            onTableRowSelected(current->row(), current->column());
        }
    });

    setCommits(makeDemoCommits(), true);
}

void CommitsPage::setCommits(const std::vector<xray::history::CommitRecord>& commits, bool isDemo) {
    rawCommits_ = commits;
    isDemo_ = isDemo;
    if (demoBadge_) {
        demoBadge_->setVisible(isDemo_);
    }
    onSelectionChanged();
}

void CommitsPage::onSelectionChanged() {
    if (rawCommits_.empty()) {
        populateTable({});
        return;
    }

    std::vector<xray::history::CommitRecord> selected = rawCommits_;
    int mode = selectionCombo_->currentData().toInt();

    try {
        if (mode == 1) { // LastN
            xray::history::LastNSelection sel(static_cast<std::size_t>(limitSpin_->value()), true);
            selected = sel.select(rawCommits_);
        } else if (mode == 2) { // Periodic
            xray::history::PeriodicSelection sel(2, 0);
            selected = sel.select(rawCommits_);
        }
    } catch (const std::exception&) {
        // Fallback to full list if selection parameters cannot be satisfied
    }

    // Apply text search
    const QString query = searchEdit_->text().trimmed().toLower();
    if (!query.isEmpty()) {
        std::vector<xray::history::CommitRecord> filtered;
        for (const auto& c : selected) {
            QString oid = QString::fromStdString(c.oid).toLower();
            QString author = QString::fromStdString(c.authorName).toLower();
            QString msg = QString::fromStdString(c.message).toLower();
            if (oid.contains(query) || author.contains(query) || msg.contains(query)) {
                filtered.push_back(c);
            }
        }
        selected = std::move(filtered);
    }

    // Apply sorting
    selected = xray::history::sortCommitsChronologically(selected, sortAscending_);
    populateTable(selected);
}

void CommitsPage::onSearchChanged(const QString&) {
    onSelectionChanged();
}

void CommitsPage::onSortToggled() {
    sortAscending_ = !sortAscending_;
    sortBtn_->setText(sortAscending_
        ? QString::fromUtf8("Сортування: найстаріші")
        : QString::fromUtf8("Сортування: найновіші"));
    onSelectionChanged();
}

void CommitsPage::onRefreshClicked() {
    onSelectionChanged();
}

void CommitsPage::onTableRowSelected(int row, int) {
    if (row >= 0 && row < static_cast<int>(displayedCommits_.size())) {
        updateDetails(row);
        emit commitSelected(QString::fromStdString(displayedCommits_[static_cast<std::size_t>(row)].oid));
    }
}

void CommitsPage::updateDetails(int rowIndex) {
    if (rowIndex < 0 || rowIndex >= static_cast<int>(displayedCommits_.size())) {
        detailsLabel_->setText(QString::fromUtf8("Виберіть коміт для перегляду деталей"));
        return;
    }

    const auto& c = displayedCommits_[static_cast<std::size_t>(rowIndex)];
    QString parents = c.parentOids.empty() ? QString::fromUtf8("корінь") : QString::fromStdString(c.parentOids.front());
    if (c.parentOids.size() > 1) {
        parents += QString::fromUtf8(" (+ %1 батьків — злиття)").arg(c.parentOids.size() - 1);
    }

    detailsLabel_->setText(QString::fromUtf8(
        "<b>Коміт:</b> %1<br>"
        "<b>Автор:</b> %2 &lt;%3&gt;<br>"
        "<b>Батьки:</b> %4<br>"
        "<b>Повідомлення:</b> %5"
    ).arg(
        QString::fromStdString(c.oid).toHtmlEscaped(),
        QString::fromStdString(c.authorName).toHtmlEscaped(),
        QString::fromStdString(c.authorEmail).toHtmlEscaped(),
        parents.toHtmlEscaped(),
        QString::fromStdString(c.message).toHtmlEscaped()
    ));
}

void CommitsPage::populateTable(const std::vector<xray::history::CommitRecord>& list) {
    displayedCommits_ = list;
    commitsTable_->setRowCount(static_cast<int>(list.size()));

    for (int r = 0; r < static_cast<int>(list.size()); ++r) {
        const auto& c = list[static_cast<std::size_t>(r)];

        auto* itemOid = new QTableWidgetItem(QString::fromStdString(c.oid));
        itemOid->setFont(QFont("monospace"));
        auto* itemAuthor = new QTableWidgetItem(QString::fromStdString(c.authorName));
        auto* itemEmail = new QTableWidgetItem(QString::fromStdString(c.authorEmail));

        auto sec = c.authoredAt.time_since_epoch().count();
        QDateTime dt = QDateTime::fromSecsSinceEpoch(sec);
        auto* itemDate = new QTableWidgetItem(dt.toString("yyyy-MM-dd hh:mm"));

        QString parentsStr;
        for (std::size_t p = 0; p < c.parentOids.size(); ++p) {
            if (p > 0) parentsStr += ", ";
            parentsStr += QString::fromStdString(c.parentOids[p]);
        }
        auto* itemParents = new QTableWidgetItem(parentsStr);
        auto* itemMsg = new QTableWidgetItem(QString::fromStdString(c.message));

        commitsTable_->setItem(r, 0, itemOid);
        commitsTable_->setItem(r, 1, itemAuthor);
        commitsTable_->setItem(r, 2, itemEmail);
        commitsTable_->setItem(r, 3, itemDate);
        commitsTable_->setItem(r, 4, itemParents);
        commitsTable_->setItem(r, 5, itemMsg);
    }

    countLabel_->setText(QString::fromUtf8("Показано комітів: %1 з %2")
        .arg(displayedCommits_.size())
        .arg(rawCommits_.size()));

    if (!displayedCommits_.empty()) {
        commitsTable_->selectRow(0);
        updateDetails(0);
    } else {
        updateDetails(-1);
    }
}

} // namespace xray::ui::history
