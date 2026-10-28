#pragma once
#include "ui/analysis/session.hpp"
#include <QWidget>
class QTableWidget;
class QComboBox;
class QLineEdit;
class QCheckBox;
class QLabel;
namespace xray::ui::analysis_pages {
class MetricsPage final : public QWidget {
    Q_OBJECT
  public:
    explicit MetricsPage(Session& session, QWidget* parent = nullptr);

  private:
    void refresh();
    void updateAggregate();
    Session& session_;
    QTableWidget* table_;
    QComboBox *metric_, *aggregation_, *direction_;
    QLineEdit* path_;
    QCheckBox* unknown_;
    QLabel* summary_;
    std::vector<xray::analysis::MetricResult> visibleMetrics_;
};
} // namespace xray::ui::analysis_pages
