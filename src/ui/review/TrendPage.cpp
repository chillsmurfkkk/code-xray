// TrendPage.cpp — D-UI2 «Динаміка»
#include "ui/review/TrendPage.hpp"
#include "ui/review/review_fixtures.hpp"

#include <QPainter>
#include <QPen>
#include <cmath>
#include <algorithm>

namespace xray::ui::review {

using namespace xray::review;

// ─── TrendChartWidget ────────────────────────────────────────

TrendChartWidget::TrendChartWidget(QWidget* parent) : QWidget(parent)
{
    setMinimumHeight(180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void TrendChartWidget::setPoints(const std::vector<TrendPoint>& points)
{
    points_ = points;
    update();
}

void TrendChartWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();
    const int pad = 40;

    // Фон
    painter.fillRect(rect(), QColor("#32302f"));

    if (points_.empty()) {
        painter.setPen(QColor("#bdae93"));
        painter.drawText(rect(), Qt::AlignCenter, QString::fromUtf8("Немає даних"));
        return;
    }

    // Знайти мін/макс
    double minVal = 1e18, maxVal = -1e18;
    for (const auto& p : points_) {
        if (p.value.has_value()) {
            minVal = std::min(minVal, *p.value);
            maxVal = std::max(maxVal, *p.value);
        }
    }
    if (minVal > maxVal) { minVal = 0; maxVal = 1; }
    if (std::abs(maxVal - minVal) < 1e-9) { minVal -= 1; maxVal += 1; }

    auto toX = [&](int i) -> int {
        if (points_.size() <= 1) return pad + (w - 2 * pad) / 2;
        return pad + static_cast<int>((w - 2 * pad) * i / (static_cast<int>(points_.size()) - 1));
    };
    auto toY = [&](double v) -> int {
        return h - pad - static_cast<int>((h - 2 * pad) * (v - minVal) / (maxVal - minVal));
    };

    // Осі
    painter.setPen(QPen(QColor("#504945"), 1));
    painter.drawLine(pad, h - pad, w - pad, h - pad);
    painter.drawLine(pad, pad, pad, h - pad);

    // Підписи значень
    painter.setPen(QColor("#bdae93"));
    auto smallFont = painter.font();
    smallFont.setPointSize(8);
    painter.setFont(smallFont);
    painter.drawText(5, pad + 4, QString::number(maxVal, 'f', 0));
    painter.drawText(5, h - pad + 4, QString::number(minVal, 'f', 0));

    // Лінія та точки — суцільні сегменти (пропуски не з'єднуємо)
    QPen linePen(QColor("#8ec07c"), 2);
    QPen dotPen(QColor("#8ec07c"), 1);
    QPen gapPen(QColor("#fb4934"), 1, Qt::DashLine);

    QPoint prev;
    bool prevValid = false;

    for (int i = 0; i < static_cast<int>(points_.size()); ++i) {
        const auto& p = points_[static_cast<std::size_t>(i)];
        int x = toX(i);

        // Підпис commit id
        painter.setPen(QColor("#bdae93"));
        painter.save();
        painter.translate(x, h - pad + 14);
        painter.rotate(-30);
        auto label = QString::fromStdString(p.sourceId);
        if (label.size() > 8) label = label.left(7) + "…";
        painter.drawText(0, 0, label);
        painter.restore();

        if (p.value.has_value()) {
            int y = toY(*p.value);
            QPoint cur(x, y);

            if (prevValid) {
                painter.setPen(linePen);
                painter.drawLine(prev, cur);
            }

            // Крапка
            painter.setPen(dotPen);
            painter.setBrush(QColor("#8ec07c"));
            painter.drawEllipse(cur, 4, 4);

            prev = cur;
            prevValid = true;
        } else {
            // Пропуск — хрестик
            painter.setPen(gapPen);
            int y = h / 2;
            painter.drawLine(x - 4, y - 4, x + 4, y + 4);
            painter.drawLine(x - 4, y + 4, x + 4, y - 4);
            prevValid = false;
        }
    }
}

// ─── TrendPage ───────────────────────────────────────────────

TrendPage::TrendPage(QWidget* parent) : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* titleLabel = new QLabel(QString::fromUtf8("Динаміка"));
    auto titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    // --- Панель керування ---
    auto* controlLayout = new QHBoxLayout;

    metricCombo_ = new QComboBox;
    metricCombo_->setMinimumHeight(32);
    metricCombo_->addItem(QString::fromUtf8("Рядки функції"), static_cast<int>(analysis::MetricId::function_lines));
    metricCombo_->addItem(QString::fromUtf8("Максимальна вкладеність"), static_cast<int>(analysis::MetricId::max_nesting));
    metricCombo_->addItem(QString::fromUtf8("Кількість розгалужень"), static_cast<int>(analysis::MetricId::branch_count));
    controlLayout->addWidget(new QLabel(QString::fromUtf8("Метрика:")));
    controlLayout->addWidget(metricCombo_);

    targetCombo_ = new QComboBox;
    targetCombo_->setMinimumHeight(32);
    targetCombo_->addItem(QString::fromUtf8("Окрема функція"), static_cast<int>(TrendAggregation::function));
    targetCombo_->addItem(QString::fromUtf8("Файл (середнє)"), static_cast<int>(TrendAggregation::file_mean));
    targetCombo_->addItem(QString::fromUtf8("Файл (максимум)"), static_cast<int>(TrendAggregation::file_max));
    controlLayout->addWidget(new QLabel(QString::fromUtf8("Ціль:")));
    controlLayout->addWidget(targetCombo_);

    buildButton_ = new QPushButton(QString::fromUtf8("Побудувати"));
    buildButton_->setMinimumHeight(32);
    controlLayout->addWidget(buildButton_);

    mainLayout->addLayout(controlLayout);

    statusLabel_ = new QLabel;
    mainLayout->addWidget(statusLabel_);

    // --- Графік ---
    chart_ = new TrendChartWidget;
    mainLayout->addWidget(chart_, /*stretch=*/2);

    // --- Таблиця точок ---
    pointsTable_ = new QTableWidget;
    pointsTable_->setColumnCount(4);
    pointsTable_->setHorizontalHeaderLabels({
        QString::fromUtf8("Коміт"),
        QString::fromUtf8("Значення"),
        QString::fromUtf8("Валідних"),
        QString::fromUtf8("Примітка"),
    });
    pointsTable_->horizontalHeader()->setStretchLastSection(true);
    pointsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    pointsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    mainLayout->addWidget(pointsTable_, /*stretch=*/1);

    // --- Сигнали ---
    connect(metricCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TrendPage::onTrendMetricChanged);
    connect(targetCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TrendPage::onTrendTargetChanged);
    connect(buildButton_, &QPushButton::clicked, this, &TrendPage::onTrendRequested);

    // Демо-дані
    setTrendReport(makeDemoTrendReport());
}

// D-H04
void TrendPage::onTrendMetricChanged()
{
    statusLabel_->setText(QString::fromUtf8("Метрику змінено — натисніть «Побудувати»."));
}

// D-H05
void TrendPage::onTrendTargetChanged()
{
    statusLabel_->setText(QString::fromUtf8("Ціль змінено — натисніть «Побудувати»."));
}

// D-H06
void TrendPage::onTrendRequested()
{
    TrendReport report;
    auto metricId = static_cast<analysis::MetricId>(metricCombo_->currentData().toInt());
    auto targetAgg = static_cast<TrendAggregation>(targetCombo_->currentData().toInt());

    report.options.metricId = metricId;
    report.options.aggregation = targetAgg;
    report.options.entityKey = (targetAgg == TrendAggregation::function) 
        ? "multiply(int,int)" : "src/math.cpp";

    if (metricId == analysis::MetricId::function_lines) {
        if (targetAgg == TrendAggregation::function) {
            report.points = {
                {"commit-aaa", 12.0, 1, 0, ""},
                {"commit-bbb", 9.0, 1, 0, ""},
                {"commit-ccc", std::nullopt, 0, 1, "Function not found"},
                {"commit-ddd", 6.0, 1, 0, ""},
                {"commit-eee", 4.0, 1, 0, ""},
            };
        } else {
            report.points = {
                {"commit-aaa", 35.0, 3, 0, "file mean"},
                {"commit-bbb", 32.0, 3, 0, "file mean"},
                {"commit-ccc", 28.0, 2, 1, "1 function skipped"},
                {"commit-ddd", 24.0, 3, 0, "file mean"},
                {"commit-eee", 20.0, 3, 0, "file mean"},
            };
        }
    } else if (metricId == analysis::MetricId::max_nesting) {
        report.points = {
            {"commit-aaa", 3.0, 1, 0, ""},
            {"commit-bbb", 2.0, 1, 0, ""},
            {"commit-ccc", std::nullopt, 0, 1, "Function not found"},
            {"commit-ddd", 1.0, 1, 0, ""},
            {"commit-eee", 0.0, 1, 0, ""},
        };
    } else { // branch_count
        report.points = {
            {"commit-aaa", 5.0, 1, 0, ""},
            {"commit-bbb", 4.0, 1, 0, ""},
            {"commit-ccc", std::nullopt, 0, 1, "Function not found"},
            {"commit-ddd", 2.0, 1, 0, ""},
            {"commit-eee", 1.0, 1, 0, ""},
        };
    }

    setTrendReport(report);
    statusLabel_->setText(QString::fromUtf8("Тренд успішно оновлено!"));
}

void TrendPage::setTrendReport(const TrendReport& report)
{
    currentTrend_ = report;
    chart_->setPoints(report.points);
    populateTable(report.points);
    statusLabel_->setText(
        QString::fromUtf8("Точок: %1, ціль: %2")
            .arg(static_cast<qlonglong>(report.points.size()))
            .arg(QString::fromStdString(report.options.entityKey)));
}

void TrendPage::populateTable(const std::vector<TrendPoint>& points)
{
    pointsTable_->setRowCount(static_cast<int>(points.size()));
    for (int row = 0; row < static_cast<int>(points.size()); ++row) {
        const auto& p = points[static_cast<std::size_t>(row)];
        auto commitLabel = QString::fromStdString(p.sourceId);
        if (commitLabel.size() > 12) commitLabel = commitLabel.left(7) + "…";
        pointsTable_->setItem(row, 0, new QTableWidgetItem(commitLabel));
        pointsTable_->setItem(row, 1, new QTableWidgetItem(
            p.value.has_value() ? QString::number(*p.value, 'f', 1) : QString::fromUtf8("—")));
        pointsTable_->setItem(row, 2, new QTableWidgetItem(QString::number(p.validFunctions)));
        pointsTable_->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(p.reason)));
    }
    pointsTable_->resizeColumnsToContents();
}

} // namespace xray::ui::review
