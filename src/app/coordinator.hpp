#pragma once

#include <QObject>
#include <QString>
#include <filesystem>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

#include "common/job.hpp"
#include "common/result.hpp"
#include "history/types.hpp"
#include "code/types.hpp"
#include "analysis/types.hpp"
#include "review/types.hpp"

class QThread;

namespace xray::app {

class Coordinator : public QObject {
    Q_OBJECT
public:
    explicit Coordinator(QObject* parent = nullptr);
    ~Coordinator() override;

    void setRepositoryPath(const std::filesystem::path& repoPath);
    const std::filesystem::path& repositoryPath() const { return repoPath_; }
    bool hasRepository() const { return !repoPath_.empty(); }

    void setAnalysisProfile(analysis::AnalysisProfile profile);
    const analysis::AnalysisProfile& analysisProfile() const { return profile_; }

    void setFileSelection(code::FileSelection selection);
    const code::FileSelection& fileSelection() const { return fileSelection_; }

    void setParseOptions(code::ParseOptions options);
    const code::ParseOptions& parseOptions() const { return parseOptions_; }

    void setComparisonOptions(review::ComparisonOptions options);
    const review::ComparisonOptions& comparisonOptions() const { return comparisonOptions_; }

    const std::vector<history::CommitRecord>& cachedCommits() const { return cachedCommits_; }
    const std::vector<history::ChangeRecord>& cachedChanges() const { return cachedChanges_; }

    // Helpers for first-parent chain validation & extraction
    static bool validateFirstParentChain(
        const std::vector<history::CommitRecord>& allCommits,
        const std::vector<std::string>& chainOids,
        std::string* errorMessage = nullptr);

    static std::vector<history::CommitRecord> extractFirstParentChain(
        const std::vector<history::CommitRecord>& allCommits,
        const std::string& tipOid,
        std::size_t limit = 20);

    // Asynchronous pipelines
    void openRepository(const std::filesystem::path& repoPath);
    void loadCommitSnapshot(const std::string& oid);
    void compareCommits(const std::string& baseOid, const std::string& targetOid);
    void buildTrend(const std::string& tipOid, std::size_t limit = 20,
                    analysis::MetricId metricId = analysis::MetricId::function_lines,
                    review::TrendAggregation agg = review::TrendAggregation::function,
                    const std::string& entityKey = {});

    void cancel();
    bool isBusy() const { return workerThread_ != nullptr; }

signals:
    void progressChanged(const xray::Progress& progress);
    void statusMessage(const QString& message, int timeoutMs = 0);
    void busyChanged(bool busy);

    void repositoryQueried(const xray::history::HistoryResult& result);
    void commitSnapshotLoaded(std::shared_ptr<const xray::code::ParsedSnapshot> parsedSnapshot);
    void comparisonCompleted(const xray::review::ComparisonReport& report);
    void trendCompleted(const xray::review::TrendReport& report);

    void operationFailed(const QString& operation, const QString& error);
    void operationCancelled(const QString& operation);

private:
    void setWorker(QThread* thread, std::stop_source stopSource, JobId id);

    std::filesystem::path repoPath_;
    analysis::AnalysisProfile profile_;
    code::FileSelection fileSelection_;
    code::ParseOptions parseOptions_;
    review::ComparisonOptions comparisonOptions_;

    std::vector<history::CommitRecord> cachedCommits_;
    std::vector<history::ChangeRecord> cachedChanges_;

    QThread* workerThread_ = nullptr;
    std::stop_source currentStopSource_;
    JobId currentJobId_ = 0;
};

} // namespace xray::app
