#pragma once

#include <QMainWindow>
#include <stop_token>
#include "code/types.hpp"
#include "common/job.hpp"

#include "ui/shared/theme.hpp"

class QThread;
class QStackedWidget;

namespace xray::ui {
class FileSelectionPage;
class SourcePage;
class StructurePage;
namespace analysis_pages { class Workspace; }
namespace review {
class ComparisonPage;
class TrendPage;
class ChangeCardPage;
class ExportPage;
}
namespace history {
class CommitsPage;
class HistoryFiltersPage;
class CodeChurnPage;
class AncestryPage;
}
}

namespace xray::app {
class AnalysisController;

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(ui::Theme theme, QWidget* parent = nullptr);
    ~MainWindow() override;
    void setSnapshot(std::shared_ptr<const code::ParsedSnapshot> snapshot, bool demonstration = false);

private:
    ui::Theme theme_;
    void loadDirectory(const QString& directory);
    void loadSource(code::SourceRequest request);

    void cancelSource();

    QThread* sourceThread_ = nullptr;
    std::stop_source sourceStop_;
    JobId sourceJobId_ = 0;

    QStackedWidget* contentStack_ = nullptr;
    ui::StructurePage* structure_ = nullptr;
    ui::SourcePage* source_ = nullptr;
    ui::FileSelectionPage* fileSelection_ = nullptr;
    AnalysisController* analysisController_ = nullptr;
    ui::analysis_pages::Workspace* analysisPages_ = nullptr;
    ui::review::ComparisonPage* comparisonPage_ = nullptr;
    ui::review::TrendPage* trendPage_ = nullptr;
    ui::review::ChangeCardPage* changeCardPage_ = nullptr;
    ui::review::ExportPage* exportPage_ = nullptr;
    ui::history::CommitsPage* commitsPage_ = nullptr;
    ui::history::HistoryFiltersPage* historyFiltersPage_ = nullptr;
    ui::history::CodeChurnPage* codeChurnPage_ = nullptr;
    ui::history::AncestryPage* ancestryPage_ = nullptr;
};

} // namespace xray::app
