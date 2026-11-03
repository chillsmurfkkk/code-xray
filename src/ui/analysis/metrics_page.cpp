#include "ui/analysis/metrics_page.hpp"
#include "ui/analysis/presentation.hpp"
#include "analysis/engine.hpp"
#include "analysis/aggregation.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QListView>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace xray::ui::analysis_pages {
namespace a = xray::analysis;
MetricsPage::MetricsPage(Session& session, QWidget* parent) : QWidget(parent), session_(session) {
    setObjectName("metricsPage");
    auto* layout = new QVBoxLayout(this);
    auto* filters = new QHBoxLayout;
    metric_ = new QComboBox;
    metric_->setProperty("role", "inputField");
    metric_->setView(new QListView);
    metric_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    metric_->setObjectName("metricSort");
    metric_->addItems({tr("Довжина (рядки)"), tr("Вкладеність"), tr("Розгалуження (синтаксичні)")});
    aggregation_ = new QComboBox;
    aggregation_->setProperty("role", "inputField");
    aggregation_->setView(new QListView);
    aggregation_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    aggregation_->setObjectName("aggregation");
    aggregation_->addItems({tr("Середнє"), tr("Максимум")});
    direction_ = new QComboBox;
    direction_->setProperty("role", "inputField");
    direction_->setView(new QListView);
    direction_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    direction_->setObjectName("metricDirection");
    direction_->addItems({tr("За спаданням"), tr("За зростанням")});
    path_ = new QLineEdit;
    path_->setProperty("role", "inputField");
    path_->setObjectName("metricPath");
    path_->setClearButtonEnabled(true);
    path_->setPlaceholderText(tr("Фільтр шляху файла…"));
    unknown_ = new QCheckBox(tr("Показувати невідомі / незастосовні"));
    unknown_->setObjectName("showUnknown");
    unknown_->setChecked(true);
    filters->addWidget(metric_);
    filters->addWidget(direction_);
    filters->addWidget(aggregation_);
    layout->addLayout(filters);
    layout->addWidget(path_);
    layout->addWidget(unknown_);
    summary_ = new QLabel;
    summary_->setObjectName("metricSummary");
    summary_->setWordWrap(true);
    layout->addWidget(summary_);
    table_ = new QTableWidget;
    table_->setObjectName("metricsTable");
    prepareTable(table_, {tr("Функція"), tr("Файл:рядок"), tr("Рядки"), tr("Вкладеність"), tr("Розгалуження"),
                          tr("Точність")});
    layout->addWidget(table_);
    auto* note = new QLabel(tr("Розгалуження рахуються синтаксично. Невідоме значення не дорівнює нулю."));
    note->setWordWrap(true);
    layout->addWidget(note);
    connect(metric_, &QComboBox::currentIndexChanged, this, &MetricsPage::refresh);
    connect(direction_, &QComboBox::currentIndexChanged, this, &MetricsPage::refresh);
    connect(aggregation_, &QComboBox::currentIndexChanged, this, &MetricsPage::updateAggregate);
    connect(path_, &QLineEdit::textChanged, this, &MetricsPage::refresh);
    connect(unknown_, &QCheckBox::toggled, this, &MetricsPage::refresh);
    connect(&session_, &Session::resultChanged, this, &MetricsPage::refresh);
    refresh();
}

void MetricsPage::refresh() {
    visibleMetrics_.clear();
    table_->setRowCount(0);
    const auto snapshot = session_.snapshot();
    if (!snapshot) {
        summary_->setText(tr("Результат аналізу ще не отримано."));
        return;
    }
    const auto index = functionIndex(*snapshot);
    std::map<std::string, std::array<const a::MetricResult*, 3>> values;
    for (const auto& value : snapshot->metrics)
        values[value.entityId][static_cast<int>(value.metricId)] = &value;
    const auto id = static_cast<a::MetricId>(metric_->currentIndex());
    const auto ranked = a::rankFunctions(*snapshot, id, direction_->currentIndex() == 0);
    table_->setUpdatesEnabled(false);
    for (const auto& entity : ranked) {
        const auto* fn = index.at(entity);
        if (!text(fn->relativePath).contains(path_->text(), Qt::CaseInsensitive))
            continue;
        const auto& metrics = values[entity];
        const auto* selected = metrics[static_cast<int>(id)];
        if (!unknown_->isChecked() && (!selected || !selected->value))
            continue;
        const int row = table_->rowCount();
        table_->insertRow(row);
        table_->setItem(row, 0, new QTableWidgetItem(text(fn->describe())));
        table_->setItem(
            row, 1,
            new QTableWidgetItem(text(fn->relativePath) + ":" + QString::number(fn->range.startLine)));
        for (int i = 0; i < 3; ++i) {
            const auto* value = metrics[i];
            auto* item = new QTableWidgetItem(value && value->value ? QString::number(*value->value)
                                                                    : QStringLiteral("—"));
            if (value) {
                item->setToolTip(validityName(value->validity) + ": " + text(value->reason));
                visibleMetrics_.push_back(*value);
            }
            table_->setItem(row, i + 2, item);
        }
        table_->setItem(row, 5, new QTableWidgetItem(validityName(fn->validity)));
    }
    fitColumns(table_);
    table_->setUpdatesEnabled(true);
    updateAggregate();
}

void MetricsPage::updateAggregate() {
    if (!session_.snapshot())
        return;
    const auto id = static_cast<a::MetricId>(metric_->currentIndex());
    const auto result = aggregation_->currentIndex() == 0
                            ? a::aggregateMetrics(visibleMetrics_, id, a::MeanReducer{})
                            : a::aggregateMetrics(visibleMetrics_, id, a::MaxReducer{});
    if (session_.snapshot()->metrics.empty()) {
        summary_->setText(tr("Аналіз завершено: функцій немає."));
        return;
    }
    if (table_->rowCount() == 0) {
        summary_->setText(
            tr("Збігів немає. Очистьте фільтр шляху або ввімкніть показ невідомих; числових даних немає."));
        return;
    }
    summary_->setText(tr("%1: %2 · числових: %3 · невідомих: %4 · незастосовних: %5 · лише синтаксично: %6 · "
                         "функцій у вибірці: %7")
                          .arg(aggregation_->currentText(), result.value
                                                                ? QString::number(*result.value, 'g', 10)
                                                                : tr("немає числових даних"))
                          .arg(result.included)
                          .arg(result.unknown)
                          .arg(result.notApplicable)
                          .arg(result.syntacticOnly)
                          .arg(table_->rowCount()));
}
} // namespace xray::ui::analysis_pages
