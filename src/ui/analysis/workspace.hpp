#pragma once
#include "ui/analysis/session.hpp"
#include "common/job.hpp"
#include <QWidget>
class QLabel;
class QStackedWidget;
class QProgressBar;
class QPushButton;
namespace xray::ui::analysis_pages {
class Workspace final : public QWidget {
    Q_OBJECT
  public:
    explicit Workspace(Session& session, QWidget* parent = nullptr);
    void setCurrentPage(int index);
    int currentPage() const;
    void setContext(const QString& context);
    void setProgress(const xray::Progress& progress);
  signals:
    void cancelRequested();
    void codeActivated(std::shared_ptr<const xray::analysis::AnalysisSnapshot> snapshot, QString entityId);

  private:
    void refreshStatus();
    Session& session_;
    QStackedWidget* pages_;
    QProgressBar* progress_;
    QPushButton* cancel_;
    QLabel* title_;
    QLabel *status_, *coverage_, *message_, *context_;
};
} // namespace xray::ui::analysis_pages
