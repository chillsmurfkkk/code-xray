#pragma once
// TrendPage.hpp — D-UI2 «Динаміка»
// Графік тренду метрики, таблиця точок.

#include "review/types.hpp"
#include "analysis/types.hpp"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <string>
#include <vector>

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

    void setTrendReport(const xray::review::TrendReport& report, bool isDemo = false);

    /// OID of the repository tip the «Побудувати» button should build from.
    void setRepositoryTip(const QString& tipOid);
    const QString& repositoryTip() const { return tipOid_; }

    /// Targets available for the current repository/snapshot. Function keys are
    /// used for function aggregation, relative paths for file aggregation.
    void setAvailableTargets(const std::vector<std::string>& functionKeys,
                             const std::vector<std::string>& filePaths);

signals:
    /// Емітується, коли користувач натискає «Побудувати» за наявності реального звіту.
    /// MainWindow з'єднує цей сигнал з Coordinator::buildTrend().
    void trendRebuildRequested(const QString& tipOid,
                               xray::analysis::MetricId metricId,
                               xray::review::TrendAggregation agg,
                               const QString& entityKey);

private slots:
    void onTrendMetricChanged();     // D-H04
    void onTrendTargetChanged();     // D-H05
    void onTrendRequested();         // D-H06

private:
    void populateTable(const std::vector<xray::review::TrendPoint>& points);
    void refreshEntityChoices(const QString& preferred = {});
    void updateTargetsFromReport(const xray::review::TrendReport& report);
    QString statusPrefix() const;

    QComboBox*        metricCombo_  = nullptr;  // Список метрики
    QComboBox*        targetCombo_  = nullptr;  // Список цілі/агрегації
    QLabel*           entityLabel_  = nullptr;  // Підпис вибору сутності
    QComboBox*        entityCombo_  = nullptr;  // Конкретна функція або файл
    QPushButton*      buildButton_  = nullptr;  // Кнопка побудови
    QTableWidget*     pointsTable_  = nullptr;  // Таблиця точок
    TrendChartWidget* chart_        = nullptr;  // Віджет графіка
    QLabel*           statusLabel_  = nullptr;
    QLabel*           demoBadge_    = nullptr;

    QString tipOid_;
    bool isDemo_ = true;
    std::vector<std::string> functionKeys_;
    std::vector<std::string> filePaths_;

    xray::review::TrendReport currentTrend_;
};

} // namespace xray::ui::review
