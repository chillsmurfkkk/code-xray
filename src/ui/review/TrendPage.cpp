// TrendPage.cpp — D-UI2 «Динаміка»
#include "ui/review/TrendPage.hpp"
#include "ui/review/review_fixtures.hpp"
#include "ui/review/review_table_delegate.hpp"

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

    const bool dark = palette().color(QPalette::Window).lightness() < 128;
    // Gruvbox green / aqua accent matching navigation rail icons (#8ec07c in dark, #427b58 in light)
    QColor greenAccent = palette().color(QPalette::Accent);
    if (!greenAccent.isValid() || greenAccent == palette().color(QPalette::Window)) {
        greenAccent = dark ? QColor("#8ec07c") : QColor("#427b58");
    }

    // Заокруглена картка графіка з акуратною рамкою
    const QRectF cardRect = QRectF(rect()).adjusted(1, 1, -1, -1);
    painter.setPen(QPen(palette().color(QPalette::Mid), 1));
    painter.setBrush(palette().color(QPalette::Base));
    painter.drawRoundedRect(cardRect, 12, 12);

    if (points_.empty()) {
        painter.setPen(palette().color(QPalette::PlaceholderText));
        painter.drawText(rect(), Qt::AlignCenter, QString::fromUtf8("Немає даних"));
        return;
    }

    const int padLeft = 52;
    const int padRight = 36;
    const int padTop = 32;
    const int padBottom = 48;

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

    const int plotW = w - padLeft - padRight;
    const int plotH = h - padTop - padBottom;
    if (plotW <= 0 || plotH <= 0) return;

    auto toX = [&](int i) -> int {
        if (points_.size() <= 1) return padLeft + plotW / 2;
        return padLeft + static_cast<int>(plotW * i / (static_cast<int>(points_.size()) - 1));
    };
    auto toY = [&](double v) -> int {
        return padTop + plotH - static_cast<int>(plotH * (v - minVal) / (maxVal - minVal));
    };

    // Підписи значень та пунктирні сітки
    auto smallFont = painter.font();
    smallFont.setPointSize(8);
    painter.setFont(smallFont);

    QPen gridPen(palette().color(QPalette::Mid), 1, Qt::DashLine);
    painter.setPen(gridPen);

    const double midVal = (maxVal + minVal) / 2.0;
    const int yMax = toY(maxVal);
    const int yMid = toY(midVal);
    const int yMin = toY(minVal);

    painter.drawLine(padLeft, yMax, w - padRight, yMax);
    painter.drawLine(padLeft, yMid, w - padRight, yMid);
    painter.drawLine(padLeft, yMin, w - padRight, yMin);

    painter.setPen(palette().color(QPalette::PlaceholderText));
    painter.drawText(QRect(4, yMax - 8, padLeft - 10, 16), Qt::AlignRight | Qt::AlignVCenter,
                     QString::number(maxVal, 'f', 1));
    painter.drawText(QRect(4, yMid - 8, padLeft - 10, 16), Qt::AlignRight | Qt::AlignVCenter,
                     QString::number(midVal, 'f', 1));
    painter.drawText(QRect(4, yMin - 8, padLeft - 10, 16), Qt::AlignRight | Qt::AlignVCenter,
                     QString::number(minVal, 'f', 1));

    // Нижня вісь
    painter.setPen(QPen(palette().color(QPalette::Mid), 1));
    painter.drawLine(padLeft, padTop + plotH, w - padRight, padTop + plotH);

    // Лінії — яскравий зелений акцент теми (Gruvbox green / aqua)
    QPen linePen(greenAccent, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    const QColor redColor(dark ? "#fb4934" : "#9d0006");
    QPen gapPen(redColor, 2, Qt::SolidLine, Qt::RoundCap);

    QPoint prev;
    bool prevValid = false;

    // Перший прохід — малюємо лінії
    for (int i = 0; i < static_cast<int>(points_.size()); ++i) {
        const auto& p = points_[static_cast<std::size_t>(i)];
        int x = toX(i);
        if (p.value.has_value()) {
            int y = toY(*p.value);
            QPoint cur(x, y);
            if (prevValid) {
                painter.setPen(linePen);
                painter.drawLine(prev, cur);
            }
            prev = cur;
            prevValid = true;
        } else {
            prevValid = false;
        }
    }

    // Другий прохід — маркери точок, підписи та пропуски
    for (int i = 0; i < static_cast<int>(points_.size()); ++i) {
        const auto& p = points_[static_cast<std::size_t>(i)];
        int x = toX(i);

        // Підпис коміту внизу
        painter.save();
        painter.translate(x, padTop + plotH + 12);
        painter.rotate(-25);
        auto label = QString::fromStdString(p.sourceId);
        if (label.size() > 10) label = label.left(9) + "…";
        painter.setPen(palette().color(QPalette::PlaceholderText));
        painter.drawText(0, 0, label);
        painter.restore();

        if (p.value.has_value()) {
            int y = toY(*p.value);
            QPoint cur(x, y);

            // Точка: зовнішній диск зеленого кольору + внутрішня серцевина фону
            painter.setPen(Qt::NoPen);
            painter.setBrush(greenAccent);
            painter.drawEllipse(cur, 5, 5);
            painter.setBrush(palette().color(QPalette::Base));
            painter.drawEllipse(cur, 2, 2);

            // Значення над точкою
            painter.setPen(palette().color(QPalette::WindowText));
            painter.drawText(QRect(x - 24, y - 20, 48, 16), Qt::AlignCenter,
                             QString::number(*p.value, 'f', 1));
        } else {
            // Пропуск — червоний хрестик та підпис (специфікація Т16)
            painter.setPen(gapPen);
            int y = padTop + plotH / 2;
            painter.drawLine(x - 5, y - 5, x + 5, y + 5);
            painter.drawLine(x - 5, y + 5, x + 5, y - 5);
            painter.setPen(redColor);
            painter.drawText(QRect(x - 30, y + 8, 60, 16), Qt::AlignCenter, QString::fromUtf8("пропуск"));
        }
    }
}

// ─── TrendPage ───────────────────────────────────────────────

TrendPage::TrendPage(QWidget* parent) : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    auto* titleLabel = new QLabel(QString::fromUtf8("Динаміка"));
    auto titleFont = titleLabel->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() + 4);
    titleFont.setWeight(QFont::Bold);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    // --- Панель керування ---
    auto* controlLayout = new QHBoxLayout;

    metricCombo_ = new QComboBox;
    metricCombo_->setMinimumHeight(32);
    metricCombo_->setProperty("role", "inputField");
    metricCombo_->addItem(QString::fromUtf8("Рядки функції"), static_cast<int>(analysis::MetricId::function_lines));
    metricCombo_->addItem(QString::fromUtf8("Максимальна вкладеність"), static_cast<int>(analysis::MetricId::max_nesting));
    metricCombo_->addItem(QString::fromUtf8("Кількість розгалужень"), static_cast<int>(analysis::MetricId::branch_count));
    controlLayout->addWidget(new QLabel(QString::fromUtf8("Метрика:")));
    controlLayout->addWidget(metricCombo_);

    targetCombo_ = new QComboBox;
    targetCombo_->setMinimumHeight(32);
    targetCombo_->setProperty("role", "inputField");
    targetCombo_->addItem(QString::fromUtf8("Окрема функція"), static_cast<int>(TrendAggregation::function));
    targetCombo_->addItem(QString::fromUtf8("Файл (середнє)"), static_cast<int>(TrendAggregation::file_mean));
    targetCombo_->addItem(QString::fromUtf8("Файл (максимум)"), static_cast<int>(TrendAggregation::file_max));
    controlLayout->addWidget(new QLabel(QString::fromUtf8("Ціль:")));
    controlLayout->addWidget(targetCombo_);

    buildButton_ = new QPushButton(QString::fromUtf8("Побудувати"));
    buildButton_->setMinimumHeight(32);
    buildButton_->setProperty("role", "primaryAction");
    controlLayout->addWidget(buildButton_);

    mainLayout->addLayout(controlLayout);

    statusLabel_ = new QLabel;
    statusLabel_->setObjectName("statusPill");
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
    setupReviewTable(pointsTable_);
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
