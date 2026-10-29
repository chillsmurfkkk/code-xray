#include "ui/history/CodeChurnPage.hpp"
#include "ui/history/history_fixtures.hpp"
#include "ui/review/review_table_delegate.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>

namespace xray::ui::history {

CodeChurnPage::CodeChurnPage(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // --- Header ---
    auto* headerLayout = new QHBoxLayout;
    auto* titleLabel = new QLabel(QString::fromUtf8("Динаміка коду (Code Churn)"));
    auto titleFont = titleLabel->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() + 4);
    titleFont.setWeight(QFont::Bold);
    titleLabel->setFont(titleFont);
    headerLayout->addWidget(titleLabel);

    completenessBadge_ = new QLabel(QString::fromUtf8("Повний підрахунок"), this);
    completenessBadge_->setObjectName("churnCompletenessBadge");
    completenessBadge_->setStyleSheet(
        "background: #b8bb26; color: #282828; font-weight: bold; "
        "border-radius: 8px; padding: 3px 8px; font-size: 11px;"
    );
    headerLayout->addWidget(completenessBadge_);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    // --- KPI Cards Bar ---
    auto* kpiLayout = new QHBoxLayout;
    kpiLayout->setSpacing(12);

    const auto makeKpiCard = [this](const QString& title, QLabel*& valLabel, const QString& objName, const QString& color) {
        auto* card = new QWidget(this);
        card->setStyleSheet("background: #32302f; border-radius: 10px; padding: 8px;");
        auto* layout = new QVBoxLayout(card);
        layout->setContentsMargins(10, 8, 10, 8);
        layout->setSpacing(4);

        auto* tLabel = new QLabel(title, card);
        tLabel->setStyleSheet("color: #a89984; font-size: 11px; text-transform: uppercase;");
        layout->addWidget(tLabel);

        valLabel = new QLabel("0", card);
        valLabel->setObjectName(objName);
        valLabel->setStyleSheet(QString("color: %1; font-size: 18px; font-weight: bold;").arg(color));
        layout->addWidget(valLabel);

        return card;
    };

    kpiLayout->addWidget(makeKpiCard(QString::fromUtf8("Додано рядків"), addedLinesLabel_, "churnAddedLabel", "#b8bb26"));
    kpiLayout->addWidget(makeKpiCard(QString::fromUtf8("Видалено рядків"), deletedLinesLabel_, "churnDeletedLabel", "#fb4934"));
    kpiLayout->addWidget(makeKpiCard(QString::fromUtf8("Чиста зміна"), netLinesLabel_, "churnNetLabel", "#8ec07c"));
    kpiLayout->addWidget(makeKpiCard(QString::fromUtf8("Змінених файлів"), filesCountLabel_, "churnFilesLabel", "#ebdbb2"));

    mainLayout->addLayout(kpiLayout);

    // --- Filter Bar ---
    auto* filterLayout = new QHBoxLayout;
    filterLayout->setSpacing(8);

    statusCombo_ = new QComboBox(this);
    statusCombo_->setObjectName("churnStatusCombo");
    statusCombo_->setMinimumHeight(32);
    statusCombo_->setProperty("role", "inputField");
    statusCombo_->addItem(QString::fromUtf8("Усі статуси"), -1);
    statusCombo_->addItem(QString::fromUtf8("Змінено (modified)"), static_cast<int>(xray::history::ChangeStatus::modified));
    statusCombo_->addItem(QString::fromUtf8("Додано (added)"), static_cast<int>(xray::history::ChangeStatus::added));
    statusCombo_->addItem(QString::fromUtf8("Видалено (deleted)"), static_cast<int>(xray::history::ChangeStatus::deleted));
    statusCombo_->addItem(QString::fromUtf8("Перейменовано (renamed)"), static_cast<int>(xray::history::ChangeStatus::renamed));
    statusCombo_->addItem(QString::fromUtf8("Тип змінено (type_changed)"), static_cast<int>(xray::history::ChangeStatus::type_changed));
    filterLayout->addWidget(statusCombo_);

    pathEdit_ = new QLineEdit(this);
    pathEdit_->setObjectName("churnPathEdit");
    pathEdit_->setPlaceholderText(QString::fromUtf8("Фільтр шляху файлу…"));
    pathEdit_->setMinimumHeight(32);
    pathEdit_->setProperty("role", "inputField");
    filterLayout->addWidget(pathEdit_, 1);

    recomputeBtn_ = new QPushButton(QString::fromUtf8("Застосувати"), this);
    recomputeBtn_->setObjectName("churnRecomputeBtn");
    recomputeBtn_->setMinimumHeight(32);
    filterLayout->addWidget(recomputeBtn_);

    resetBtn_ = new QPushButton(QString::fromUtf8("Скинути"), this);
    resetBtn_->setObjectName("churnResetBtn");
    resetBtn_->setMinimumHeight(32);
    filterLayout->addWidget(resetBtn_);

    mainLayout->addLayout(filterLayout);

    // --- Table ---
    changesTable_ = new QTableWidget(this);
    changesTable_->setObjectName("churnChangesTable");
    changesTable_->setColumnCount(6);
    changesTable_->setHorizontalHeaderLabels({
        QString::fromUtf8("Статус"),
        QString::fromUtf8("Шлях файлу"),
        QString::fromUtf8("Додано (+)"),
        QString::fromUtf8("Видалено (-)"),
        QString::fromUtf8("Бінарний"),
        QString::fromUtf8("Коміт OID")
    });
    changesTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    changesTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    changesTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    changesTable_->verticalHeader()->setVisible(false);
    changesTable_->horizontalHeader()->setStretchLastSection(true);
    changesTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    changesTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    changesTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    changesTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    changesTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    changesTable_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    changesTable_->setItemDelegate(new xray::ui::review::RoundedRowDelegate(changesTable_));
    mainLayout->addWidget(changesTable_, 1);

    connect(statusCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CodeChurnPage::onFilterChanged);
    connect(pathEdit_, &QLineEdit::textChanged, this, &CodeChurnPage::onFilterChanged);
    connect(recomputeBtn_, &QPushButton::clicked, this, &CodeChurnPage::onFilterChanged);
    connect(resetBtn_, &QPushButton::clicked, this, &CodeChurnPage::onResetFilter);

    setChanges(makeDemoChanges());
}

void CodeChurnPage::setChanges(const std::vector<xray::history::ChangeRecord>& changes) {
    rawChanges_ = changes;
    onResetFilter();
}

QString CodeChurnPage::statusToString(xray::history::ChangeStatus status) {
    switch (status) {
    case xray::history::ChangeStatus::added: return QString::fromUtf8("Додано");
    case xray::history::ChangeStatus::deleted: return QString::fromUtf8("Видалено");
    case xray::history::ChangeStatus::modified: return QString::fromUtf8("Змінено");
    case xray::history::ChangeStatus::renamed: return QString::fromUtf8("Перейменовано");
    case xray::history::ChangeStatus::type_changed: return QString::fromUtf8("Тип змінено");
    }
    return QString::fromUtf8("Невідомо");
}

void CodeChurnPage::onFilterChanged() {
    std::vector<xray::history::ChangeRecord> filtered = rawChanges_;

    int statusData = statusCombo_->currentData().toInt();
    if (statusData >= 0) {
        auto target = static_cast<xray::history::ChangeStatus>(statusData);
        filtered = xray::history::filterChangesByStatus(filtered, target);
    }

    QString pathQuery = pathEdit_->text().trimmed().toLower();
    if (!pathQuery.isEmpty()) {
        std::vector<xray::history::ChangeRecord> pathMatches;
        for (const auto& ch : filtered) {
            QString path = ch.newPath.has_value() ? QString::fromStdString(*ch.newPath)
                         : ch.oldPath.has_value() ? QString::fromStdString(*ch.oldPath) : "";
            if (path.toLower().contains(pathQuery)) {
                pathMatches.push_back(ch);
            }
        }
        filtered = std::move(pathMatches);
    }

    updateKpis(filtered);
    populateTable(filtered);
}

void CodeChurnPage::onResetFilter() {
    statusCombo_->setCurrentIndex(0);
    pathEdit_->clear();
    updateKpis(rawChanges_);
    populateTable(rawChanges_);
}

void CodeChurnPage::updateKpis(const std::vector<xray::history::ChangeRecord>& filtered) {
    currentChurn_ = xray::history::computeCodeChurn(filtered);

    addedLinesLabel_->setText(QString("+%1").arg(currentChurn_.totalAddedLines));
    deletedLinesLabel_->setText(QString("-%1").arg(currentChurn_.totalDeletedLines));

    QString netPrefix = currentChurn_.netLines > 0 ? "+" : "";
    netLinesLabel_->setText(QString("%1%2").arg(netPrefix).arg(currentChurn_.netLines));

    filesCountLabel_->setText(QString("%1 (бінарних: %2)")
        .arg(currentChurn_.modifiedFilesCount)
        .arg(currentChurn_.binaryFilesCount));

    if (currentChurn_.hasUnknownLineCounts) {
        completenessBadge_->setText(QString::fromUtf8("Є невідомі значення (nullopt)"));
        completenessBadge_->setStyleSheet(
            "background: #fabd2f; color: #282828; font-weight: bold; "
            "border-radius: 8px; padding: 3px 8px; font-size: 11px;"
        );
    } else {
        completenessBadge_->setText(QString::fromUtf8("Повний підрахунок"));
        completenessBadge_->setStyleSheet(
            "background: #b8bb26; color: #282828; font-weight: bold; "
            "border-radius: 8px; padding: 3px 8px; font-size: 11px;"
        );
    }
}

void CodeChurnPage::populateTable(const std::vector<xray::history::ChangeRecord>& list) {
    displayedChanges_ = list;
    changesTable_->setRowCount(static_cast<int>(list.size()));

    for (int r = 0; r < static_cast<int>(list.size()); ++r) {
        const auto& ch = list[static_cast<std::size_t>(r)];

        auto* itemStatus = new QTableWidgetItem(statusToString(ch.status));
        QString path = ch.newPath.has_value() ? QString::fromStdString(*ch.newPath)
                     : ch.oldPath.has_value() ? QString::fromStdString(*ch.oldPath) : QString::fromUtf8("(невідомо)");
        auto* itemPath = new QTableWidgetItem(path);

        QString addedStr = ch.addedLines.has_value() ? QString("+%1").arg(*ch.addedLines) : "-";
        auto* itemAdded = new QTableWidgetItem(addedStr);
        if (ch.addedLines.has_value() && *ch.addedLines > 0) {
            itemAdded->setForeground(QColor("#b8bb26"));
        }

        QString deletedStr = ch.deletedLines.has_value() ? QString("-%1").arg(*ch.deletedLines) : "-";
        auto* itemDeleted = new QTableWidgetItem(deletedStr);
        if (ch.deletedLines.has_value() && *ch.deletedLines > 0) {
            itemDeleted->setForeground(QColor("#fb4934"));
        }

        auto* itemBin = new QTableWidgetItem(ch.isBinary ? QString::fromUtf8("Так") : QString::fromUtf8("Ні"));
        auto* itemOid = new QTableWidgetItem(QString::fromStdString(ch.commitOid));
        itemOid->setFont(QFont("monospace"));

        changesTable_->setItem(r, 0, itemStatus);
        changesTable_->setItem(r, 1, itemPath);
        changesTable_->setItem(r, 2, itemAdded);
        changesTable_->setItem(r, 3, itemDeleted);
        changesTable_->setItem(r, 4, itemBin);
        changesTable_->setItem(r, 5, itemOid);
    }
}

} // namespace xray::ui::history
