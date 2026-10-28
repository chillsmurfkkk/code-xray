// ChangeCardPage.cpp — D-UI3 «Картка зміни»
#include "ui/review/ChangeCardPage.hpp"
#include "ui/review/review_table_delegate.hpp"
#include "ui/code/code_view.hpp"

#include <QFont>
#include <QHeaderView>
#include <QMessageBox>

namespace xray::ui::review {

using namespace xray::review;

static QString metricName(analysis::MetricId id) {
    switch (id) {
    case analysis::MetricId::function_lines: return QString::fromUtf8("Рядки");
    case analysis::MetricId::max_nesting:    return QString::fromUtf8("Вкладеність");
    case analysis::MetricId::branch_count:   return QString::fromUtf8("Розгалуження");
    }
    return "?";
}

static QString findingStateName(FindingState state) {
    switch (state) {
    case FindingState::new_finding:       return QString::fromUtf8("Нове");
    case FindingState::resolved:          return QString::fromUtf8("Усунено");
    case FindingState::existing:          return QString::fromUtf8("Наявне");
    case FindingState::removed_with_code: return QString::fromUtf8("Видалено з кодом");
    case FindingState::unknown:           return QString::fromUtf8("Невідомо");
    }
    return "?";
}

static QString ruleIdName(analysis::RuleId id) {
    switch (id) {
    case analysis::RuleId::long_function:  return "long_function";
    case analysis::RuleId::deep_nesting:   return "deep_nesting";
    case analysis::RuleId::many_branches:  return "many_branches";
    }
    return "?";
}

ChangeCardPage::ChangeCardPage(QWidget* parent) : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    auto* headerLayout = new QHBoxLayout;
    backButton_ = new QPushButton(QString::fromUtf8("← Назад"));
    backButton_->setProperty("role", "primaryAction");
    backButton_->setMinimumHeight(30);
    backButton_->setToolTip(QString::fromUtf8("Повернутися до таблиці порівняння"));
    connect(backButton_, &QPushButton::clicked, this, &ChangeCardPage::backToComparisonRequested);
    headerLayout->addWidget(backButton_);

    titleLabel_ = new QLabel(QString::fromUtf8("Картка зміни"));
    auto titleFont = titleLabel_->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    titleLabel_->setFont(titleFont);
    headerLayout->addWidget(titleLabel_);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    explanationLabel_ = new QLabel;
    explanationLabel_->setWordWrap(true);
    mainLayout->addWidget(explanationLabel_);

    // --- Код до/після (splitter) ---
    auto* codeSplitter = new QSplitter(Qt::Horizontal);
    codeSplitter->setHandleWidth(6);
    codeSplitter->setChildrenCollapsible(false);

    auto* beforeBox = new QVBoxLayout;
    beforeBox->addWidget(new QLabel(QString::fromUtf8("Код «до»")));
    beforeCodeView_ = new xray::ui::CodeView(this);
    auto* beforeWidget = new QWidget;
    beforeBox->addWidget(beforeCodeView_);
    beforeWidget->setLayout(beforeBox);

    auto* afterBox = new QVBoxLayout;
    afterBox->addWidget(new QLabel(QString::fromUtf8("Код «після»")));
    afterCodeView_ = new xray::ui::CodeView(this);
    auto* afterWidget = new QWidget;
    afterBox->addWidget(afterCodeView_);
    afterWidget->setLayout(afterBox);

    codeSplitter->addWidget(beforeWidget);
    codeSplitter->addWidget(afterWidget);
    mainLayout->addWidget(codeSplitter, /*stretch=*/3);

    // --- Нижня панель: метрики + порушення + кнопка ---
    auto* bottomLayout = new QHBoxLayout;

    // Таблиця метрик
    metricsTable_ = new QTableWidget;
    metricsTable_->setColumnCount(4);
    metricsTable_->setHorizontalHeaderLabels({
        QString::fromUtf8("Метрика"),
        QString::fromUtf8("До"),
        QString::fromUtf8("Після"),
        QString::fromUtf8("Різниця"),
    });
    metricsTable_->horizontalHeader()->setStretchLastSection(true);
    setupReviewTable(metricsTable_);
    connect(metricsTable_, &QTableWidget::cellClicked, this, &ChangeCardPage::onMetricRowSelected);
    bottomLayout->addWidget(metricsTable_);

    // Список порушень
    auto* findingsLayout = new QVBoxLayout;
    findingsLayout->addWidget(new QLabel(QString::fromUtf8("Порушення")));
    findingsList_ = new QListWidget;
    connect(findingsList_, &QListWidget::currentRowChanged, this, &ChangeCardPage::onRelatedFindingSelected);
    findingsLayout->addWidget(findingsList_);

    historyButton_ = new QPushButton(QString::fromUtf8("Історія файла"));
    historyButton_->setMinimumHeight(32);
    historyButton_->setProperty("role", "primaryAction");
    connect(historyButton_, &QPushButton::clicked, this, &ChangeCardPage::onFileHistoryRequested);
    findingsLayout->addWidget(historyButton_);

    bottomLayout->addLayout(findingsLayout);
    mainLayout->addLayout(bottomLayout, /*stretch=*/1);

    // Порожній стан
    explanationLabel_->setText(QString::fromUtf8("Виберіть елемент у таблиці порівняння."));
}

void ChangeCardPage::showItem(const ReviewItem& item, const ComparisonReport& report)
{
    currentItem_ = item;

    const bool isDemo = (report.analysisFingerprint == "fp-demo-v1" || report.baseId == "analysis-base-001");
    if (isDemo) {
        titleLabel_->setText(QString::fromUtf8("[Демо] Картка зміни: ") + QString::fromStdString(item.entityKey));
    } else {
        titleLabel_->setText(QString::fromUtf8("Картка зміни: ") + QString::fromStdString(item.entityKey));
    }
    explanationLabel_->setText(QString::fromStdString(item.explanation));

    // Обов'язково очищаємо обидві панелі коду перед встановленням нового
    beforeCodeView_->setSourceText("");
    afterCodeView_->setSourceText("");

    // --- Код до/після з урахуванням шляху файла ---
    auto extractCode = [&](const std::string& sourceId, const std::string& relativePath, const code::SourceRange& range) -> QString {
        for (const auto& snap : {report.base, report.target}) {
            if (!snap || !snap->parsed || !snap->parsed->source) continue;
            if (snap->parsed->source->id != sourceId) continue;
            for (const auto& file : snap->parsed->source->files) {
                // Шлях обов'язково має збігатися з відносним шляхом посилання
                bool matches = false;
                if (relativePath.empty()) {
                    matches = true;
                } else if (file.relativePath == relativePath) {
                    matches = true;
                } else {
                    auto f1 = std::filesystem::path(file.relativePath).generic_string();
                    auto f2 = std::filesystem::path(relativePath).generic_string();
                    if (f1 == f2 || f1.ends_with("/" + f2) || f2.ends_with("/" + f1)) {
                        matches = true;
                    }
                }
                if (!matches) continue;

                if (file.bytes) {
                    const auto totalSize = file.bytes->size();
                    auto startByte = static_cast<std::string::size_type>(range.startByte);
                    auto endByte = static_cast<std::string::size_type>(range.endByte);
                    if (startByte < totalSize && endByte <= totalSize && startByte < endByte) {
                        return QString::fromStdString(file.bytes->substr(startByte, endByte - startByte));
                    }
                    if (startByte < totalSize) {
                        return QString::fromStdString(file.bytes->substr(startByte));
                    }
                    return QString::fromStdString(*file.bytes);
                }
            }
        }
        return QString::fromUtf8("(код недоступний)");
    };

    auto isTargetSource = [&](const std::string& sourceId) -> bool {
        return report.target && report.target->parsed && report.target->parsed->source &&
               report.target->parsed->source->id == sourceId;
    };

    if (item.kind == ReviewKind::added) {
        beforeCodeView_->setSourceText(QString::fromUtf8("// (функції ще не існувало у версії «до»)\n"));
        if (!item.sourceRefs.empty()) {
            const auto& ref = item.sourceRefs.front();
            afterCodeView_->setSourceText(extractCode(ref.sourceId, ref.relativePath, ref.range));
            currentPath_ = QString::fromStdString(ref.relativePath);
        }
    } else if (item.kind == ReviewKind::removed) {
        if (!item.sourceRefs.empty()) {
            const auto& ref = item.sourceRefs.front();
            beforeCodeView_->setSourceText(extractCode(ref.sourceId, ref.relativePath, ref.range));
            currentPath_ = QString::fromStdString(ref.relativePath);
        }
        afterCodeView_->setSourceText(QString::fromUtf8("// (функцію видалено у версії «після»)\n"));
    } else if (item.kind == ReviewKind::unmatched) {
        // [P2] unmatched: якщо посилання на target, то показуємо в «після», а зліва — заглушку
        if (!item.sourceRefs.empty()) {
            const auto& ref = item.sourceRefs.front();
            currentPath_ = QString::fromStdString(ref.relativePath);
            if (isTargetSource(ref.sourceId)) {
                beforeCodeView_->setSourceText(QString::fromUtf8("// (незіставлено: сутність присутня лише у версії «після»)\n"));
                afterCodeView_->setSourceText(extractCode(ref.sourceId, ref.relativePath, ref.range));
            } else {
                beforeCodeView_->setSourceText(extractCode(ref.sourceId, ref.relativePath, ref.range));
                afterCodeView_->setSourceText(QString::fromUtf8("// (незіставлено: сутність присутня лише у версії «до»)\n"));
            }
        } else {
            beforeCodeView_->setSourceText(QString::fromUtf8("// (незіставлено: код «до» відсутній)\n"));
            afterCodeView_->setSourceText(QString::fromUtf8("// (незіставлено: код «після» відсутній)\n"));
        }
    } else {
        // changed / unchanged
        if (item.sourceRefs.size() >= 2) {
            const auto* baseRef = &item.sourceRefs[0];
            const auto* targetRef = &item.sourceRefs[1];
            if (isTargetSource(baseRef->sourceId) && !isTargetSource(targetRef->sourceId)) {
                std::swap(baseRef, targetRef);
            }
            beforeCodeView_->setSourceText(extractCode(baseRef->sourceId, baseRef->relativePath, baseRef->range));
            afterCodeView_->setSourceText(extractCode(targetRef->sourceId, targetRef->relativePath, targetRef->range));
            currentPath_ = QString::fromStdString(targetRef->relativePath.empty() ? baseRef->relativePath : targetRef->relativePath);
        } else if (!item.sourceRefs.empty()) {
            const auto& ref = item.sourceRefs.front();
            currentPath_ = QString::fromStdString(ref.relativePath);
            if (isTargetSource(ref.sourceId)) {
                beforeCodeView_->setSourceText(QString::fromUtf8("// (версія «до» недоступна)\n"));
                afterCodeView_->setSourceText(extractCode(ref.sourceId, ref.relativePath, ref.range));
            } else {
                beforeCodeView_->setSourceText(extractCode(ref.sourceId, ref.relativePath, ref.range));
                afterCodeView_->setSourceText(QString::fromUtf8("// (версія «після» недоступна)\n"));
            }
        }
    }

    // --- Метрики ---
    metricsTable_->setRowCount(static_cast<int>(item.metrics.size()));
    for (int row = 0; row < static_cast<int>(item.metrics.size()); ++row) {
        const auto& m = item.metrics[static_cast<std::size_t>(row)];
        metricsTable_->setItem(row, 0, new QTableWidgetItem(metricName(m.metricId)));
        metricsTable_->setItem(row, 1, new QTableWidgetItem(
            m.before.has_value() ? QString::number(*m.before) : QString::fromUtf8("—")));
        metricsTable_->setItem(row, 2, new QTableWidgetItem(
            m.after.has_value() ? QString::number(*m.after) : QString::fromUtf8("—")));
        metricsTable_->setItem(row, 3, new QTableWidgetItem(
            m.delta.has_value() ? QString::number(*m.delta) : QString::fromUtf8("—")));
    }
    metricsTable_->resizeColumnsToContents();

    // --- Порушення ---
    findingsList_->clear();
    for (const auto& f : item.findings) {
        findingsList_->addItem(
            ruleIdName(f.ruleId) + " — " + findingStateName(f.state));
    }
    if (item.findings.empty()) {
        findingsList_->addItem(QString::fromUtf8("(немає порушень)"));
    }
}

// D-H07
void ChangeCardPage::onMetricRowSelected(int row, int /*column*/)
{
    if (row < 0 || row >= static_cast<int>(currentItem_.metrics.size())) return;
    const auto& m = currentItem_.metrics[static_cast<std::size_t>(row)];
    explanationLabel_->setText(
        metricName(m.metricId) + ": " +
        (m.before.has_value() ? QString::number(*m.before) : "?") + " → " +
        (m.after.has_value() ? QString::number(*m.after) : "?"));
}

// D-H08
void ChangeCardPage::onRelatedFindingSelected(int row)
{
    if (row < 0 || row >= static_cast<int>(currentItem_.findings.size())) return;
    const auto& f = currentItem_.findings[static_cast<std::size_t>(row)];
    explanationLabel_->setText(
        QString::fromUtf8("Порушення: ") + ruleIdName(f.ruleId) +
        " — " + findingStateName(f.state));
}

// D-H09
void ChangeCardPage::onFileHistoryRequested()
{
    if (currentPath_.isEmpty()) return;
    emit fileHistoryRequested(currentPath_, QString());
}

} // namespace xray::ui::review
