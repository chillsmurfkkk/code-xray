#pragma once
// TrendPage.hpp — D-UI2 «Динаміка»
// Графік тренду метрики, таблиця точок.

#include "review/types.hpp"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace xray::ui::review {

/// Простий віджет-графік для TrendReport.
class TrendChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit TrendChartWidget(QWidget* parent = nullptr);
    void setPoints(const std::vector<xray::review::TrendPoint>& points);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<xray::review::TrendPoint> points_;
};

class TrendPage : public QWidget {
    Q_OBJECT
public:
    explicit TrendPage(QWidget* parent = nullptr);

    void setTrendReport(const xray::review::TrendReport& report);

private slots:
    void onTrendMetricChanged();     // D-H04
    void onTrendTargetChanged();     // D-H05
    void onTrendRequested();         // D-H06

private:
    void populateTable(const std::vector<xray::review::TrendPoint>& points);

    QComboBox*        metricCombo_  = nullptr;  // Список метрики
    QComboBox*        targetCombo_  = nullptr;  // Список цілі/агрегації
    QPushButton*      buildButton_  = nullptr;  // Кнопка побудови
    QTableWidget*     pointsTable_  = nullptr;  // Таблиця точок
    TrendChartWidget* chart_        = nullptr;  // Віджет графіка
    QLabel*           statusLabel_  = nullptr;

    xray::review::TrendReport currentTrend_;
};

} // namespace xray::ui::review
