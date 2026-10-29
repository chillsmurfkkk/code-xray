#include "ui/history/AncestryPage.hpp"
#include "ui/history/history_fixtures.hpp"
#include "ui/review/review_table_delegate.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>

namespace xray::ui::history {

AncestryPage::AncestryPage(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(14);

    // --- Header ---
    auto* headerLayout = new QHBoxLayout;
    auto* titleLabel = new QLabel(QString::fromUtf8("Дерево ревізій та аналіз родоводу"));
    auto titleFont = titleLabel->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() + 4);
    titleFont.setWeight(QFont::Bold);
    titleLabel->setFont(titleFont);
    headerLayout->addWidget(titleLabel);

    demoBadge_ = new QLabel(QString::fromUtf8("Демо-режим"), this);
    demoBadge_->setObjectName("ancestryDemoBadge");
    demoBadge_->setStyleSheet(
        "background: #d65d0e; color: #fbf1c7; font-weight: bold; "
        "border-radius: 8px; padding: 3px 8px; font-size: 11px;"
    );
    demoBadge_->setToolTip(QString::fromUtf8("Відображаються демонстраційні фікстури ревізій."));
    headerLayout->addWidget(demoBadge_);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    // --- Section 1: isAncestor ---
    auto* ancestorBox = new QGroupBox(QString::fromUtf8("Перевірка відношення предківства (isAncestor)"), this);
    ancestorBox->setObjectName("ancestorBox");
    auto* aLayout = new QGridLayout(ancestorBox);
    aLayout->setContentsMargins(12, 12, 12, 12);
    aLayout->setSpacing(8);

    aLayout->addWidget(new QLabel(QString::fromUtf8("OID предка (ancestor):")), 0, 0);
    ancestorOidEdit_ = new QLineEdit(this);
    ancestorOidEdit_->setObjectName("ancestorOidEdit");
    ancestorOidEdit_->setPlaceholderText(QString::fromUtf8("Наприклад: d1a5090"));
    ancestorOidEdit_->setProperty("role", "inputField");
    aLayout->addWidget(ancestorOidEdit_, 0, 1);

    aLayout->addWidget(new QLabel(QString::fromUtf8("OID нащадка (descendant):")), 0, 2);
    descendantOidEdit_ = new QLineEdit(this);
    descendantOidEdit_->setObjectName("descendantOidEdit");
    descendantOidEdit_->setPlaceholderText(QString::fromUtf8("Наприклад: c6f81a2"));
    descendantOidEdit_->setProperty("role", "inputField");
    aLayout->addWidget(descendantOidEdit_, 0, 3);

    checkAncestryBtn_ = new QPushButton(QString::fromUtf8("Перевірити зв'язок"), this);
    checkAncestryBtn_->setObjectName("checkAncestryBtn");
    checkAncestryBtn_->setMinimumHeight(32);
    checkAncestryBtn_->setProperty("role", "primaryAction");
    aLayout->addWidget(checkAncestryBtn_, 0, 4);

    ancestryResultLabel_ = new QLabel(QString::fromUtf8("Введіть OID комітів для перевірки"), this);
    ancestryResultLabel_->setObjectName("ancestryResultLabel");
    ancestryResultLabel_->setStyleSheet("background: palette(base); color: palette(window-text); border: 1px solid palette(mid); padding: 8px; border-radius: 6px;");
    aLayout->addWidget(ancestryResultLabel_, 1, 0, 1, 5);

    mainLayout->addWidget(ancestorBox);

    // --- Section 2: LinearHistorySelection ---
    auto* linearBox = new QGroupBox(QString::fromUtf8("Лінійний ланцюг історії першого батька (LinearHistorySelection)"), this);
    linearBox->setObjectName("linearBox");
    auto* lLayout = new QVBoxLayout(linearBox);
    lLayout->setContentsMargins(12, 12, 12, 12);
    lLayout->setSpacing(8);

    auto* lTopLayout = new QHBoxLayout;
    lTopLayout->addWidget(new QLabel(QString::fromUtf8("Стартовий OID ланцюга:")));
    linearStartEdit_ = new QLineEdit(this);
    linearStartEdit_->setObjectName("linearStartEdit");
    linearStartEdit_->setPlaceholderText(QString::fromUtf8("Початковий OID для обходу"));
    linearStartEdit_->setProperty("role", "inputField");
    lTopLayout->addWidget(linearStartEdit_, 1);

    traceLinearBtn_ = new QPushButton(QString::fromUtf8("Побудувати ланцюг"), this);
    traceLinearBtn_->setObjectName("traceLinearBtn");
    traceLinearBtn_->setMinimumHeight(32);
    traceLinearBtn_->setProperty("role", "primaryAction");
    lTopLayout->addWidget(traceLinearBtn_);
    lLayout->addLayout(lTopLayout);

    linearStatusLabel_ = new QLabel(this);
    linearStatusLabel_->setObjectName("linearStatusLabel");
    linearStatusLabel_->setStyleSheet("color: palette(window-text); padding: 2px;");
    lLayout->addWidget(linearStatusLabel_);

    linearTable_ = new QTableWidget(this);
    linearTable_->setObjectName("linearHistoryTable");
    linearTable_->setColumnCount(5);
    linearTable_->setHorizontalHeaderLabels({
        QString::fromUtf8("Крок #"),
        QString::fromUtf8("OID"),
        QString::fromUtf8("Автор"),
        QString::fromUtf8("Батьки"),
        QString::fromUtf8("Повідомлення")
    });
    xray::ui::review::setupReviewTable(linearTable_);
    linearTable_->horizontalHeader()->setStretchLastSection(true);
    linearTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    linearTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    linearTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    linearTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    lLayout->addWidget(linearTable_, 1);

    mainLayout->addWidget(linearBox, 1);

    connect(checkAncestryBtn_, &QPushButton::clicked, this, &AncestryPage::onCheckAncestry);
    connect(traceLinearBtn_, &QPushButton::clicked, this, &AncestryPage::onTraceLinearHistory);

    setCommits(makeDemoCommits(), true);
}

void AncestryPage::setCommits(const std::vector<xray::history::CommitRecord>& commits, bool isDemo) {
    rawCommits_ = commits;
    isDemo_ = isDemo;
    if (demoBadge_) {
        demoBadge_->setVisible(isDemo_);
    }
    if (rawCommits_.empty()) {
        ancestorOidEdit_->clear();
        descendantOidEdit_->clear();
        linearStartEdit_->clear();
        ancestryResultLabel_->setStyleSheet("background: palette(base); color: palette(window-text); border: 1px solid palette(mid); padding: 8px; border-radius: 6px;");
        ancestryResultLabel_->setText(QString::fromUtf8("Історія порожня. Введіть OID комітів для перевірки"));
        linearStatusLabel_->clear();
        populateLinearTable({});
        return;
    }

    ancestorOidEdit_->setText(QString::fromStdString(rawCommits_.back().oid));
    descendantOidEdit_->setText(QString::fromStdString(rawCommits_.front().oid));
    linearStartEdit_->setText(QString::fromStdString(rawCommits_.front().oid));
    onTraceLinearHistory();
}

void AncestryPage::onCheckAncestry() {
    QString ancestor = ancestorOidEdit_->text().trimmed();
    QString descendant = descendantOidEdit_->text().trimmed();
    const bool dark = palette().color(QPalette::Window).lightness() < 128;

    try {
        bool result = xray::history::isAncestor(rawCommits_, ancestor.toStdString(), descendant.toStdString());
        if (result) {
            const QString bg = dark ? "#b8bb26" : "#427b58";
            const QString fg = dark ? "#282828" : "#fbf1c7";
            ancestryResultLabel_->setStyleSheet(QString("background: %1; color: %2; font-weight: bold; padding: 8px; border-radius: 6px;").arg(bg, fg));
            ancestryResultLabel_->setText(QString::fromUtf8("✓ Підтверджено: коміт %1 є предком коміта %2 у графі ревізій")
                .arg(ancestor.toHtmlEscaped(), descendant.toHtmlEscaped()));
        } else {
            ancestryResultLabel_->setStyleSheet("background: palette(alternate-base); color: palette(window-text); border: 1px solid palette(mid); padding: 8px; border-radius: 6px;");
            ancestryResultLabel_->setText(QString::fromUtf8("✗ Відхилено: коміт %1 НЕ є предком коміта %2")
                .arg(ancestor.toHtmlEscaped(), descendant.toHtmlEscaped()));
        }
    } catch (const std::exception& e) {
        const QString bg = dark ? "#fb4934" : "#9d0006";
        const QString fg = dark ? "#282828" : "#fbf1c7";
        ancestryResultLabel_->setStyleSheet(QString("background: %1; color: %2; font-weight: bold; padding: 8px; border-radius: 6px;").arg(bg, fg));
        ancestryResultLabel_->setText(QString::fromUtf8("Помилка перевірки предківства: %1")
            .arg(QString::fromUtf8(e.what()).toHtmlEscaped()));
    }
}

void AncestryPage::onTraceLinearHistory() {
    QString start = linearStartEdit_->text().trimmed();
    const bool dark = palette().color(QPalette::Window).lightness() < 128;

    if (start.isEmpty()) {
        linearStatusLabel_->setStyleSheet("color: palette(window-text); padding: 2px;");
        linearStatusLabel_->setText(QString::fromUtf8("Введіть стартовий OID"));
        populateLinearTable({});
        return;
    }

    try {
        xray::history::LinearHistorySelection linSel(start.toStdString());
        auto chain = linSel.select(rawCommits_);

        const QString successColor = dark ? "#b8bb26" : "#427b58";
        linearStatusLabel_->setStyleSheet(QString("color: %1; font-weight: bold; padding: 2px;").arg(successColor));
        linearStatusLabel_->setText(QString::fromUtf8("Успішно побудовано first-parent ланцюг: %1 кроків від %2")
            .arg(QString::number(chain.size()), start.toHtmlEscaped()));
        populateLinearTable(chain);

    } catch (const std::exception& e) {
        const QString errColor = dark ? "#fb4934" : "#9d0006";
        linearStatusLabel_->setStyleSheet(QString("color: %1; font-weight: bold; padding: 2px;").arg(errColor));
        linearStatusLabel_->setText(QString::fromUtf8("Помилка побудови ланцюга: %1")
            .arg(QString::fromUtf8(e.what()).toHtmlEscaped()));
        populateLinearTable({});
    }
}

void AncestryPage::populateLinearTable(const std::vector<xray::history::CommitRecord>& chain) {
    linearTable_->setRowCount(static_cast<int>(chain.size()));

    for (int r = 0; r < static_cast<int>(chain.size()); ++r) {
        const auto& c = chain[static_cast<std::size_t>(r)];

        auto* itemStep = new QTableWidgetItem(QString("#%1").arg(r + 1));
        auto* itemOid = new QTableWidgetItem(QString::fromStdString(c.oid));
        itemOid->setFont(QFont("monospace"));
        auto* itemAuthor = new QTableWidgetItem(QString::fromStdString(c.authorName));

        QString parentsStr;
        for (std::size_t p = 0; p < c.parentOids.size(); ++p) {
            if (p > 0) parentsStr += ", ";
            parentsStr += QString::fromStdString(c.parentOids[p]);
        }
        auto* itemParents = new QTableWidgetItem(parentsStr);
        auto* itemMsg = new QTableWidgetItem(QString::fromStdString(c.message));

        linearTable_->setItem(r, 0, itemStep);
        linearTable_->setItem(r, 1, itemOid);
        linearTable_->setItem(r, 2, itemAuthor);
        linearTable_->setItem(r, 3, itemParents);
        linearTable_->setItem(r, 4, itemMsg);
    }
}

} // namespace xray::ui::history
