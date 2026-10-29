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
#include <QEvent>

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

    demoBadge_ = new QLabel(QString::fromUtf8("Демо-режим"), this);
    demoBadge_->setObjectName("churnDemoBadge");
    demoBadge_->setStyleSheet(
        "background: #d65d0e; color: #fbf1c7; font-weight: bold; "
        "border-radius: 8px; padding: 3px 8px; font-size: 11px;"
    );
    demoBadge_->setToolTip(QString::fromUtf8("Відображаються демонстраційні зміни файлів."));
    headerLayout->addWidget(demoBadge_);

    completenessBadge_ = new QLabel(QString::fromUtf8("Повний підрахунок"), this);
    completenessBadge_->setObjectName("churnCompletenessBadge");
    completenessBadge_->setStyleSheet(
        "background: #427b58; color: #fbf1c7; font-weight: bold; "
        "border-radius: 8px; padding: 3px 8px; font-size: 11px;"
    );
    headerLayout->addWidget(completenessBadge_);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    // --- KPI Cards Bar ---
    auto* kpiLayout = new QHBoxLayout;
    kpiLayout->setSpacing(12);

    const auto makeKpiCard = [this](const QString& title, QLabel*& valLabel, const QString& objName) {
        auto* card = new QWidget(this);
        card->setStyleSheet("background: palette(base); border: 1px solid palette(mid); border-radius: 10px; padding: 8px;");
        auto* layout = new QVBoxLayout(card);
        layout->setContentsMargins(10, 8, 10, 8);
        layout->setSpacing(4);

        auto* tLabel = new QLabel(title, card);
        tLabel->setStyleSheet("color: palette(placeholder-text); font-size: 11px; text-transform: uppercase; font-weight: bold;");
        layout->addWidget(tLabel);

        valLabel = new QLabel("0", card);
        valLabel->setObjectName(objName);
        valLabel->setStyleSheet("font-size: 18px; font-weight: bold;");
        layout->addWidget(valLabel);

        return card;
    };

    kpiLayout->addWidget(makeKpiCard(QString::fromUtf8("Додано рядків"), addedLinesLabel_, "churnAddedLabel"));
    kpiLayout->addWidget(makeKpiCard(QString::fromUtf8("Видалено рядків"), deletedLinesLabel_, "churnDeletedLabel"));
    kpiLayout->addWidget(makeKpiCard(QString::fromUtf8("Чиста зміна"), netLinesLabel_, "churnNetLabel"));
    kpiLayout->addWidget(makeKpiCard(QString::fromUtf8("Змінених файлів"), filesCountLabel_, "churnFilesLabel"));

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
    pathEdit_->setPlaceholderText(QString::fromUtf8("Фільтр шляху файлу (новий або старий)…"));
    pathEdit_->setMinimumHeight(32);
    pathEdit_->setProperty("role", "inputField");
    filterLayout->addWidget(pathEdit_, 1);

    recomputeBtn_ = new QPushButton(QString::fromUtf8("Застосувати"), this);
    recomputeBtn_->setObjectName("churnRecomputeBtn");
    recomputeBtn_->setMinimumHeight(32);
    recomputeBtn_->setProperty("role", "primaryAction");
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
    xray::ui::review::setupReviewTable(changesTable_);
    changesTable_->horizontalHeader()->setStretchLastSection(true);
    changesTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    changesTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    changesTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    changesTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    changesTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    changesTable_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    mainLayout->addWidget(changesTable_, 1);

    connect(statusCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CodeChurnPage::onFilterChanged);
    connect(pathEdit_, &QLineEdit::textChanged, this, &CodeChurnPage::onFilterChanged);
    connect(recomputeBtn_, &QPushButton::clicked, this, &CodeChurnPage::onFilterChanged);
    connect(resetBtn_, &QPushButton::clicked, this, &CodeChurnPage::onResetFilter);

    setChanges(makeDemoChanges(), true);
}

void CodeChurnPage::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) {
        updateKpis(displayedChanges_);
        populateTable(displayedChanges_);
    }
}

void CodeChurnPage::setChanges(const std::vector<xray::history::ChangeRecord>& changes, bool isDemo) {
    rawChanges_ = changes;
    isDemo_ = isDemo;
    if (demoBadge_) {
        demoBadge_->setVisible(isDemo_);
    }
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
            bool matches = false;
            if (ch.newPath.has_value() && QString::fromStdString(*ch.newPath).toLower().contains(pathQuery)) {
                matches = true;
            }
            if (ch.oldPath.has_value() && QString::fromStdString(*ch.oldPath).toLower().contains(pathQuery)) {
                matches = true;
            }
            if (matches) {
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
    const bool dark = palette().color(QPalette::Window).lightness() < 128;

    const QString addColor = dark ? "#b8bb26" : "#427b58";
    const QString delColor = dark ? "#fb4934" : "#9d0006";
    const QString netColor = dark ? "#8ec07c" : "#427b58";

    addedLinesLabel_->setText(QString("+%1").arg(currentChurn_.totalAddedLines));
    addedLinesLabel_->setStyleSheet(QString("color: %1; font-size: 18px; font-weight: bold;").arg(addColor));

    deletedLinesLabel_->setText(QString("-%1").arg(currentChurn_.totalDeletedLines));
    deletedLinesLabel_->setStyleSheet(QString("color: %1; font-size: 18px; font-weight: bold;").arg(delColor));

    QString netPrefix = currentChurn_.netLines > 0 ? "+" : "";
    netLinesLabel_->setText(QString("%1%2").arg(netPrefix).arg(currentChurn_.netLines));
    netLinesLabel_->setStyleSheet(QString("color: %1; font-size: 18px; font-weight: bold;").arg(netColor));

    filesCountLabel_->setText(QString("%1 (бінарних: %2)")
        .arg(currentChurn_.modifiedFilesCount)
        .arg(currentChurn_.binaryFilesCount));
    filesCountLabel_->setStyleSheet("color: palette(window-text); font-size: 18px; font-weight: bold;");

    if (currentChurn_.hasUnknownLineCounts) {
        completenessBadge_->setText(QString::fromUtf8("Є невідомі значення (nullopt)"));
        const QString bg = dark ? "#fabd2f" : "#b57614";
        const QString fg = dark ? "#282828" : "#fbf1c7";
        completenessBadge_->setStyleSheet(
            QString("background: %1; color: %2; font-weight: bold; border-radius: 8px; padding: 3px 8px; font-size: 11px;")
                .arg(bg, fg)
        );
    } else {
        completenessBadge_->setText(QString::fromUtf8("Повний підрахунок"));
        const QString bg = dark ? "#b8bb26" : "#427b58";
        const QString fg = dark ? "#282828" : "#fbf1c7";
        completenessBadge_->setStyleSheet(
            QString("background: %1; color: %2; font-weight: bold; border-radius: 8px; padding: 3px 8px; font-size: 11px;")
                .arg(bg, fg)
        );
    }
}

void CodeChurnPage::populateTable(const std::vector<xray::history::ChangeRecord>& list) {
    displayedChanges_ = list;
    changesTable_->setRowCount(static_cast<int>(list.size()));
    const bool dark = palette().color(QPalette::Window).lightness() < 128;

    for (int r = 0; r < static_cast<int>(list.size()); ++r) {
        const auto& ch = list[static_cast<std::size_t>(r)];

        auto* itemStatus = new QTableWidgetItem(statusToString(ch.status));

        QString path;
        if (ch.status == xray::history::ChangeStatus::renamed && ch.oldPath.has_value() && ch.newPath.has_value()) {
            path = QString("%1 → %2")
                .arg(QString::fromStdString(*ch.oldPath), QString::fromStdString(*ch.newPath));
        } else if (ch.newPath.has_value()) {
            path = QString::fromStdString(*ch.newPath);
        } else if (ch.oldPath.has_value()) {
            path = QString::fromStdString(*ch.oldPath);
        } else {
            path = QString::fromUtf8("(невідомо)");
        }
        auto* itemPath = new QTableWidgetItem(path);

        QString addedStr = ch.addedLines.has_value() ? QString("+%1").arg(*ch.addedLines) : "-";
        auto* itemAdded = new QTableWidgetItem(addedStr);
        if (ch.addedLines.has_value() && *ch.addedLines > 0) {
            itemAdded->setForeground(QColor(dark ? "#b8bb26" : "#427b58"));
        }

        QString deletedStr = ch.deletedLines.has_value() ? QString("-%1").arg(*ch.deletedLines) : "-";
        auto* itemDeleted = new QTableWidgetItem(deletedStr);
        if (ch.deletedLines.has_value() && *ch.deletedLines > 0) {
            itemDeleted->setForeground(QColor(dark ? "#fb4934" : "#9d0006"));
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
