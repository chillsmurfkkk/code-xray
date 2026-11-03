#include "ui/analysis/rules_page.hpp"
#include "ui/analysis/presentation.hpp"
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QStyle>
#include <QVBoxLayout>

namespace xray::ui::analysis_pages {
RulesPage::RulesPage(Session& session, QWidget* parent) : QWidget(parent), session_(session) {
    setObjectName("rulesPage");
    auto* layout = new QVBoxLayout(this);
    auto* note = new QLabel(tr("Правило спрацьовує при перевищенні порогу. Після змін повторіть аналіз."));
    note->setWordWrap(true);
    layout->addWidget(note);
    table_ = new QTableWidget(3, 3);
    table_->setObjectName("rulesTable");
    prepareTable(table_, {tr("Увімкнено"), tr("Правило"), tr("Метрика")});
    for (int i = 0; i < 3; ++i) {
        auto* item = new QTableWidgetItem;
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
        table_->setItem(i, 0, item);
        table_->setItem(i, 1, new QTableWidgetItem(ruleName(i)));
        table_->setItem(
            i, 2, new QTableWidgetItem(QStringList{"function_lines", "max_nesting", "branch_count"}[i]));
    }
    fitColumns(table_);
    layout->addWidget(table_);
    auto* form = new QFormLayout;
    for (int i = 0; i < 3; ++i) {
        limits_[i] = new QLineEdit;
        limits_[i]->setProperty("role", "inputField");
        limits_[i]->setObjectName(QString("threshold%1").arg(i));
        limits_[i]->setMaxLength(32);
        form->addRow(ruleName(i) + tr(" — поріг:"), limits_[i]);
        connect(limits_[i], &QLineEdit::textChanged, this, &RulesPage::readDraft);
    }
    layout->addLayout(form);
    error_ = new QLabel;
    error_->setObjectName("rulesError");
    error_->setWordWrap(true);
    layout->addWidget(error_);
    apply_ = new QPushButton(tr("Застосувати та повторити аналіз"));
    apply_->setObjectName("applyRules");
    apply_->setProperty("role", "primaryAction");
    layout->addWidget(apply_);
    layout->addStretch();
    connect(table_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (item->column() == 0)
            readDraft();
    });
    connect(apply_, &QPushButton::clicked, this, &RulesPage::onRulesApplied);
    connect(&session_, &Session::profileChanged, this, &RulesPage::loadProfile);
    connect(&session_, &Session::stateChanged, this,
            [this] { apply_->setEnabled(session_.draftValid() && session_.hasInput() && !session_.busy()); });
    loadProfile();
}

void RulesPage::loadProfile() {
    const auto& profile = session_.profile();
    const std::array<std::int64_t, 3> values{profile.longFunctionLimit, profile.nestingLimit,
                                             profile.branchLimit};
    const QSignalBlocker blocker(table_);
    for (int i = 0; i < 3; ++i) {
        const QSignalBlocker editBlocker(limits_[i]);
        table_->item(i, 0)->setCheckState(profile.enabledRules[i] ? Qt::Checked : Qt::Unchecked);
        limits_[i]->setText(QString::number(values[i]));
        limits_[i]->setProperty("invalid", false);
    }
    error_->clear();
    apply_->setEnabled(session_.hasInput() && !session_.busy());
}

bool RulesPage::readDraft() {
    std::array<bool, 3> enabled{};
    std::array<std::int64_t, 3> values{};
    static const QRegularExpression digits(QStringLiteral("^[0-9]+$"));
    bool valid = true;
    for (int i = 0; i < 3; ++i) {
        bool ok = false;
        values[i] = limits_[i]->text().toLongLong(&ok);
        ok = ok && digits.match(limits_[i]->text()).hasMatch() && values[i] >= 0;
        limits_[i]->setProperty("invalid", !ok);
        valid = valid && ok;
        enabled[i] = table_->item(i, 0)->checkState() == Qt::Checked;
    }
    error_->setText(
        valid ? QString{}
              : tr("Кожен поріг має бути цілим числом від 0 до 9223372036854775807. Профіль не змінено."));
    if (valid)
        session_.editRules(enabled, values);
    else
        session_.setDraftValid(false);
    return valid;
}

void RulesPage::onRulesApplied() {
    if (readDraft())
        session_.requestAnalysis();
}
} // namespace xray::ui::analysis_pages
