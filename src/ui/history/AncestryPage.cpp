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
    checkAncestryBtn_->setStyleSheet("background: #8ec07c; color: #282828; font-weight: bold;");
    aLayout->addWidget(checkAncestryBtn_, 0, 4);

    ancestryResultLabel_ = new QLabel(QString::fromUtf8("Введіть OID комітів для перевірки"), this);
    ancestryResultLabel_->setObjectName("ancestryResultLabel");
    ancestryResultLabel_->setStyleSheet("background: #32302f; color: #ebdbb2; padding: 8px; border-radius: 6px;");
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
    lTopLayout->addWidget(traceLinearBtn_);
    lLayout->addLayout(lTopLayout);

    linearStatusLabel_ = new QLabel(this);
    linearStatusLabel_->setObjectName("linearStatusLabel");
    linearStatusLabel_->setStyleSheet("color: #ebdbb2; padding: 2px;");
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
    linearTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    linearTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    linearTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    linearTable_->verticalHeader()->setVisible(false);
    linearTable_->horizontalHeader()->setStretchLastSection(true);
    linearTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    linearTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    linearTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    linearTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    linearTable_->setItemDelegate(new xray::ui::review::RoundedRowDelegate(linearTable_));
    lLayout->addWidget(linearTable_, 1);

    mainLayout->addWidget(linearBox, 1);

    connect(checkAncestryBtn_, &QPushButton::clicked, this, &AncestryPage::onCheckAncestry);
    connect(traceLinearBtn_, &QPushButton::clicked, this, &AncestryPage::onTraceLinearHistory);

    setCommits(makeDemoCommits());
}

void AncestryPage::setCommits(const std::vector<xray::history::CommitRecord>& commits) {
    rawCommits_ = commits;
    if (!rawCommits_.empty()) {
        ancestorOidEdit_->setText(QString::fromStdString(rawCommits_.back().oid));
        descendantOidEdit_->setText(QString::fromStdString(rawCommits_.front().oid));
        linearStartEdit_->setText(QString::fromStdString(rawCommits_.front().oid));
        onTraceLinearHistory();
    }
}

void AncestryPage::onCheckAncestry() {
    QString ancestor = ancestorOidEdit_->text().trimmed();
    QString descendant = descendantOidEdit_->text().trimmed();

    try {
        bool result = xray::history::isAncestor(rawCommits_, ancestor.toStdString(), descendant.toStdString());
        if (result) {
            ancestryResultLabel_->setStyleSheet("background: #b8bb26; color: #282828; font-weight: bold; padding: 8px; border-radius: 6px;");
            ancestryResultLabel_->setText(QString::fromUtf8("✓ Підтверджено: коміт %1 є предком коміта %2 у графі ревізій").arg(ancestor, descendant));
        } else {
            ancestryResultLabel_->setStyleSheet("background: #504945; color: #ebdbb2; padding: 8px; border-radius: 6px;");
            ancestryResultLabel_->setText(QString::fromUtf8("✗ Відхилено: коміт %1 НЕ є предком коміта %2").arg(ancestor, descendant));
        }
    } catch (const std::exception& e) {
        ancestryResultLabel_->setStyleSheet("background: #fb4934; color: #282828; font-weight: bold; padding: 8px; border-radius: 6px;");
        ancestryResultLabel_->setText(QString::fromUtf8("Помилка перевірки предківства: %1").arg(e.what()));
    }
}

void AncestryPage::onTraceLinearHistory() {
    QString start = linearStartEdit_->text().trimmed();
    if (start.isEmpty()) {
        linearStatusLabel_->setText(QString::fromUtf8("Введіть стартовий OID"));
        populateLinearTable({});
        return;
    }

    try {
        xray::history::LinearHistorySelection linSel(start.toStdString());
        auto chain = linSel.select(rawCommits_);

        linearStatusLabel_->setStyleSheet("color: #b8bb26;");
        linearStatusLabel_->setText(QString::fromUtf8("Успішно побудовано first-parent ланцюг: %1 кроків від %2")
            .arg(chain.size()).arg(start));
        populateLinearTable(chain);

    } catch (const std::exception& e) {
        linearStatusLabel_->setStyleSheet("color: #fb4934;");
        linearStatusLabel_->setText(QString::fromUtf8("Помилка побудови ланцюга: %1").arg(e.what()));
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
