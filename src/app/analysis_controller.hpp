#pragma once
#include "ui/analysis/session.hpp"
#include "common/job.hpp"
#include <stop_token>

class QThread;
namespace xray::app {
class AnalysisController final : public QObject {
    Q_OBJECT
  public:
    explicit AnalysisController(QObject* parent = nullptr);
    ~AnalysisController() override;
    ui::analysis_pages::Session& session() { return session_; }
    void setInput(std::shared_ptr<const code::ParsedSnapshot> input);
    void cancel();
  signals:
    void progressChanged(const xray::Progress& progress);

  private:
    void start(analysis::AnalysisProfile profile);
    ui::analysis_pages::Session session_;
    std::shared_ptr<const code::ParsedSnapshot> input_;
    QThread* thread_ = nullptr;
    std::stop_source stop_;
    JobId generation_ = 0;
    bool pending_ = false;
};
} // namespace xray::app
