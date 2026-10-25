// ChangeCardPage.cpp — D-UI3 «Картка зміни»
#include "ui/review/ChangeCardPage.hpp"

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
    case FindingState::new_finding:      return QString::fromUtf8("Нове");
    case FindingState::resolved:         return QString::fromUtf8("Усунено");
    case FindingState::existing:         return QString::fromUtf8("Наявне");
    case FindingState::removed_with_code: return QString::fromUtf8("Видалено з кодом");
    case FindingState::unknown:          return QString::fromUtf8("Невідомо");
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

    titleLabel_ = new QLabel(QString::fromUtf8("Картка зміни"));
    auto titleFont = titleLabel_->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    titleLabel_->setFont(titleFont);
    mainLayout->addWidget(titleLabel_);

    explanationLabel_ = new QLabel;
    explanationLabel_->setWordWrap(true);
    mainLayout->addWidget(explanationLabel_);

    // --- Код до/після (splitter) ---
    auto* codeSplitter = new QSplitter(Qt::Horizontal);
    codeSplitter->setHandleWidth(6);
    codeSplitter->setChildrenCollapsible(false);
    codeSplitter->setStyleSheet("QSplitter::handle { background-color: #504945; }");

    auto* beforeBox = new QVBoxLayout;
    beforeBox->addWidget(new QLabel(QString::fromUtf8("Код «до»")));
    beforeCodeEdit_ = new QTextEdit;
    beforeCodeEdit_->setReadOnly(true);
    beforeCodeEdit_->setFont(QFont("Consolas", 10));
    beforeCodeEdit_->setLineWrapMode(QTextEdit::NoWrap);
    auto* beforeWidget = new QWidget;
    beforeBox->addWidget(beforeCodeEdit_);
    beforeWidget->setLayout(beforeBox);

    auto* afterBox = new QVBoxLayout;
    afterBox->addWidget(new QLabel(QString::fromUtf8("Код «після»")));
    afterCodeEdit_ = new QTextEdit;
    afterCodeEdit_->setReadOnly(true);
    afterCodeEdit_->setFont(QFont("Consolas", 10));
    afterCodeEdit_->setLineWrapMode(QTextEdit::NoWrap);
    auto* afterWidget = new QWidget;
    afterBox->addWidget(afterCodeEdit_);
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
    metricsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    metricsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
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

    titleLabel_->setText(QString::fromUtf8("Картка зміни: ") + QString::fromStdString(item.entityKey));
    explanationLabel_->setText(QString::fromStdString(item.explanation));

    // --- Код до/після ---
    auto extractCode = [&](const std::string& sourceId, const code::SourceRange& range) -> QString {
        // Шукаємо байти знімка у звіті
        for (const auto& snap : {report.base, report.target}) {
            if (!snap || !snap->parsed || !snap->parsed->source) continue;
            if (snap->parsed->source->id != sourceId) continue;
            for (const auto& file : snap->parsed->source->files) {
                if (file.bytes) {
                    auto startByte = static_cast<std::string::size_type>(range.startByte);
                    auto endByte = static_cast<std::string::size_type>(range.endByte);
                    if (endByte <= file.bytes->size()) {
                        return QString::fromStdString(file.bytes->substr(startByte, endByte - startByte));
                    }
                    return QString::fromStdString(*file.bytes);
                }
            }
        }
        return QString::fromUtf8("(код недоступний)");
    };

    beforeCodeEdit_->clear();
    afterCodeEdit_->clear();

    if (item.kind == ReviewKind::added) {
        beforeCodeEdit_->setPlainText(QString::fromUtf8("// (функції ще не існувало у версії «до»)"));
        if (!item.sourceRefs.empty()) {
            const auto& ref = item.sourceRefs.front();
            afterCodeEdit_->setPlainText(extractCode(ref.sourceId, ref.range));
            currentPath_ = QString::fromStdString(ref.relativePath);
        }
    } else if (item.kind == ReviewKind::removed) {
        if (!item.sourceRefs.empty()) {
            const auto& ref = item.sourceRefs.front();
            beforeCodeEdit_->setPlainText(extractCode(ref.sourceId, ref.range));
            currentPath_ = QString::fromStdString(ref.relativePath);
        }
        afterCodeEdit_->setPlainText(QString::fromUtf8("// (функцію видалено у версії «після»)"));
    } else {
        if (!item.sourceRefs.empty()) {
            const auto& ref = item.sourceRefs.front();
            beforeCodeEdit_->setPlainText(extractCode(ref.sourceId, ref.range));
            currentPath_ = QString::fromStdString(ref.relativePath);
        }
        if (item.sourceRefs.size() > 1) {
            const auto& ref = item.sourceRefs[1];
            afterCodeEdit_->setPlainText(extractCode(ref.sourceId, ref.range));
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
