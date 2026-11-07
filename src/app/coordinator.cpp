#include "app/coordinator.hpp"

#include "history/api.hpp"
#include "code/api.hpp"
#include "analysis/api.hpp"
#include "review/api.hpp"

#include <QThread>
#include <QMetaObject>
#include <exception>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

namespace xray::app {

Coordinator::Coordinator(QObject* parent)
    : QObject(parent)
{
}

Coordinator::~Coordinator() {
    // Stop every worker before waiting: a superseded thread may still be running
    // and must never be destroyed while active (QThread aborts in that case).
    currentStopSource_.request_stop();
    for (auto& pending : pendingThreads_) {
        pending.stopSource.request_stop();
    }

    if (workerThread_) {
        workerThread_->wait();
    }
    for (auto& pending : pendingThreads_) {
        if (pending.thread) {
            pending.thread->wait();
        }
    }
    pendingThreads_.clear();
}

void Coordinator::setRepositoryPath(const std::filesystem::path& repoPath) {
    repoPath_ = repoPath;
}

void Coordinator::setAnalysisProfile(analysis::AnalysisProfile profile) {
    profile_ = std::move(profile);
}

void Coordinator::setFileSelection(code::FileSelection selection) {
    fileSelection_ = std::move(selection);
}

void Coordinator::setParseOptions(code::ParseOptions options) {
    parseOptions_ = std::move(options);
}

void Coordinator::setComparisonOptions(review::ComparisonOptions options) {
    comparisonOptions_ = std::move(options);
}

void Coordinator::cancel() {
    if (workerThread_) {
        currentStopSource_.request_stop();
        emit statusMessage(QString::fromUtf8("Скасовуємо операцію…"), 3000);
    }
}

void Coordinator::setWorker(QThread* thread, std::stop_source stopSource, JobId id) {
    // Cancel the previous operation without blocking the GUI thread. The old
    // thread (and its stop source) stays tracked until it finishes so the
    // destructor can request cancellation and wait for it safely.
    if (workerThread_) {
        currentStopSource_.request_stop();
        pendingThreads_.push_back({workerThread_, currentStopSource_});
    }
    workerThread_ = thread;
    currentStopSource_ = std::move(stopSource);
    currentJobId_ = id;
    emit busyChanged(true);

    connect(thread, &QThread::finished, this, [this, thread, id] {
        pendingThreads_.erase(
            std::remove_if(pendingThreads_.begin(), pendingThreads_.end(),
                           [thread](const PendingWorker& pending) {
                               return pending.thread == thread;
                           }),
            pendingThreads_.end());
        if (id == currentJobId_) {
            workerThread_ = nullptr;
            emit busyChanged(false);
        }
        // Schedule deletion only after the bookkeeping above has run, so no
        // pendingThreads_ entry can outlive its QThread object.
        thread->deleteLater();
    });
}

bool Coordinator::validateFirstParentChain(
    const std::vector<history::CommitRecord>& allCommits,
    const std::vector<std::string>& chainOids,
    std::string* errorMessage)
{
    if (chainOids.empty()) {
        if (errorMessage) *errorMessage = "Ланцюжок комітів для тренду порожній.";
        return false;
    }
    if (chainOids.size() == 1) {
        return true;
    }

    std::unordered_map<std::string, const history::CommitRecord*> commitMap;
    commitMap.reserve(allCommits.size());
    for (const auto& c : allCommits) {
        commitMap[c.oid] = &c;
    }

    auto firstIt = commitMap.find(chainOids[0]);
    auto secondIt = commitMap.find(chainOids[1]);
    if (firstIt == commitMap.end()) {
        if (errorMessage) *errorMessage = "Коміт не знайдено в репозиторії: " + chainOids[0];
        return false;
    }
    if (secondIt == commitMap.end()) {
        if (errorMessage) *errorMessage = "Коміт не знайдено в репозиторії: " + chainOids[1];
        return false;
    }

    bool newestFirst = true;
    if (!firstIt->second->parentOids.empty() && firstIt->second->parentOids.front() == chainOids[1]) {
        newestFirst = true;
    } else if (!secondIt->second->parentOids.empty() && secondIt->second->parentOids.front() == chainOids[0]) {
        newestFirst = false;
    } else {
        const history::CommitRecord* cur = firstIt->second;
        bool found = false;
        while (!cur->parentOids.empty()) {
            const auto& nextOid = cur->parentOids.front();
            if (nextOid == chainOids[1]) {
                found = true;
                break;
            }
            auto it = commitMap.find(nextOid);
            if (it == commitMap.end()) break;
            cur = it->second;
        }
        if (found) {
            newestFirst = true;
        } else {
            cur = secondIt->second;
            while (!cur->parentOids.empty()) {
                const auto& nextOid = cur->parentOids.front();
                if (nextOid == chainOids[0]) {
                    found = true;
                    break;
                }
                auto it = commitMap.find(nextOid);
                if (it == commitMap.end()) break;
                cur = it->second;
            }
            if (found) {
                newestFirst = false;
            } else {
                if (errorMessage) {
                    *errorMessage = "Коміти " + chainOids[0] + " та " + chainOids[1] +
                                    " не належать до єдиного first-parent ланцюжка.";
                }
                return false;
            }
        }
    }

    for (std::size_t i = 1; i < chainOids.size(); ++i) {
        const std::string& parentExpected = newestFirst ? chainOids[i] : chainOids[i - 1];
        const std::string& childExpected  = newestFirst ? chainOids[i - 1] : chainOids[i];

        auto childIt = commitMap.find(childExpected);
        if (childIt == commitMap.end()) {
            if (errorMessage) *errorMessage = "Коміт не знайдено: " + childExpected;
            return false;
        }

        const history::CommitRecord* cur = childIt->second;
        bool stepFound = false;
        while (!cur->parentOids.empty()) {
            const auto& nextOid = cur->parentOids.front();
            if (nextOid == parentExpected) {
                stepFound = true;
                break;
            }
            auto it = commitMap.find(nextOid);
            if (it == commitMap.end()) break;
            cur = it->second;
        }

        if (!stepFound) {
            if (errorMessage) {
                *errorMessage = "Коміт " + parentExpected + " не є першим предком (first-parent) для " + childExpected;
            }
            return false;
        }
    }

    return true;
}

std::vector<history::CommitRecord> Coordinator::extractFirstParentChain(
    const std::vector<history::CommitRecord>& allCommits,
    const std::string& tipOid,
    std::size_t limit)
{
    if (allCommits.empty() || tipOid.empty() || limit == 0) {
        return {};
    }

    std::unordered_map<std::string, const history::CommitRecord*> commitMap;
    commitMap.reserve(allCommits.size());
    for (const auto& c : allCommits) {
        commitMap[c.oid] = &c;
    }

    auto it = commitMap.find(tipOid);
    if (it == commitMap.end()) {
        return {};
    }

    std::vector<history::CommitRecord> chain;
    const history::CommitRecord* cur = it->second;
    std::unordered_set<std::string> visited;

    while (cur != nullptr && chain.size() < limit) {
        if (!visited.insert(cur->oid).second) {
            break;
        }
        chain.push_back(*cur);
        if (cur->parentOids.empty()) {
            break;
        }
        const auto& nextOid = cur->parentOids.front();
        auto nextIt = commitMap.find(nextOid);
        if (nextIt == commitMap.end()) {
            break;
        }
        cur = nextIt->second;
    }

    return chain;
}

void Coordinator::openRepository(const std::filesystem::path& repoPath) {
    repoPath_ = repoPath;
    const auto jobId = ++currentJobId_;
    std::stop_source stopSource;
    const auto token = stopSource.get_token();

    emit repositoryOpening();
    emit statusMessage(QString::fromUtf8("Читаємо журнал комітів Git…"));

    auto thread = QThread::create([this, repoPath, jobId, token]() {
        using QueryRes = Result<history::HistoryResult>;
        auto result = std::make_shared<QueryRes>(Cancelled{});

        try {
            JobContext ctx;
            ctx.jobId = jobId;
            ctx.stopToken = token;
            ctx.onProgress = [this, jobId](const Progress& p) {
                QMetaObject::invokeMethod(this, [this, jobId, p] {
                    if (jobId == currentJobId_ && !currentStopSource_.stop_requested()) {
                        emit progressChanged(p);
                    }
                }, Qt::QueuedConnection);
            };

            history::RepositorySpec spec{repoPath};
            history::HistoryRequest req;
            req.limit = 100;

            *result = history::query(spec, req, ctx);
        } catch (const std::exception& e) {
            *result = Error{ErrorCode::read_error, e.what()};
        } catch (...) {
            *result = Error{ErrorCode::read_error, "Невідома помилка читання репозиторію"};
        }

        QMetaObject::invokeMethod(this, [this, jobId, result]() {
            if (jobId != currentJobId_) {
                return;
            }
            if (currentStopSource_.stop_requested() || std::holds_alternative<Cancelled>(*result)) {
                emit operationCancelled(QString::fromUtf8("Відкриття репозиторію"));
                return;
            }
            if (std::holds_alternative<Error>(*result)) {
                const auto& err = std::get<Error>(*result);
                emit operationFailed(QString::fromUtf8("Відкриття репозиторію"), QString::fromStdString(err.message));
            } else if (auto* histRes = std::get_if<history::HistoryResult>(result.get())) {
                cachedCommits_ = histRes->commits;
                cachedChanges_ = histRes->changes;
                emit repositoryQueried(*histRes);
                emit statusMessage(QString::fromUtf8("Git-репозиторій відкрито: знайдено %1 комітів, %2 змін")
                    .arg(histRes->commits.size()).arg(histRes->changes.size()), 5000);
            }
        }, Qt::QueuedConnection);
    });

    thread->setParent(this);
    setWorker(thread, std::move(stopSource), jobId);
    thread->start();
}

void Coordinator::queryFileHistory(const std::string& relativePath, const std::string& startOid, std::size_t limit) {
    if (repoPath_.empty()) {
        emit operationFailed(QString::fromUtf8("Історія файла"), QString::fromUtf8("Репозиторій не вибрано"));
        return;
    }

    const auto jobId = ++currentJobId_;
    std::stop_source stopSource;
    const auto token = stopSource.get_token();

    emit statusMessage(QString::fromUtf8("Завантажуємо історію файла %1…").arg(QString::fromStdString(relativePath)));

    const auto repoPath = repoPath_;
    auto thread = QThread::create([this, repoPath, relativePath, startOid, limit, jobId, token]() {
        using QueryRes = Result<history::HistoryResult>;
        auto result = std::make_shared<QueryRes>(Cancelled{});

        try {
            JobContext ctx;
            ctx.jobId = jobId;
            ctx.stopToken = token;
            ctx.onProgress = [this, jobId](const Progress& p) {
                QMetaObject::invokeMethod(this, [this, jobId, p] {
                    if (jobId == currentJobId_ && !currentStopSource_.stop_requested()) {
                        emit progressChanged(p);
                    }
                }, Qt::QueuedConnection);
            };

            history::RepositorySpec spec{repoPath};
            history::HistoryRequest req;
            req.startOid = startOid;
            req.relativePath = relativePath;
            req.limit = (limit == 0 ? 1000 : limit);

            *result = history::query(spec, req, ctx);
        } catch (const std::exception& e) {
            *result = Error{ErrorCode::read_error, e.what()};
        } catch (...) {
            *result = Error{ErrorCode::read_error, "Невідома помилка читання історії файла"};
        }

        QMetaObject::invokeMethod(this, [this, jobId, relativePath, startOid, result]() {
            if (jobId != currentJobId_) {
                return;
            }
            if (currentStopSource_.stop_requested() || std::holds_alternative<Cancelled>(*result)) {
                emit operationCancelled(QString::fromUtf8("Історія файла"));
                return;
            }
            if (std::holds_alternative<Error>(*result)) {
                const auto& err = std::get<Error>(*result);
                emit operationFailed(QString::fromUtf8("Історія файла"), QString::fromStdString(err.message));
            } else if (auto* histRes = std::get_if<history::HistoryResult>(result.get())) {
                emit fileHistoryQueried(QString::fromStdString(relativePath), QString::fromStdString(startOid), *histRes);
                const QString msg = histRes->hasMore
                    ? QString::fromUtf8("Історію файла оновлено: перші %1 змін (є ще)").arg(histRes->changes.size())
                    : QString::fromUtf8("Історію файла оновлено: знайдено %1 змін").arg(histRes->changes.size());
                emit statusMessage(msg, 5000);
            }
        }, Qt::QueuedConnection);
    });

    thread->setParent(this);
    setWorker(thread, std::move(stopSource), jobId);
    thread->start();
}

void Coordinator::loadCommitSnapshot(const std::string& oid) {
    if (repoPath_.empty()) {
        emit operationFailed(QString::fromUtf8("Завантаження знімку"), QString::fromUtf8("Репозиторій не вибрано"));
        return;
    }

    const auto jobId = ++currentJobId_;
    std::stop_source stopSource;
    const auto token = stopSource.get_token();

    emit statusMessage(QString::fromUtf8("Завантажуємо знімок ревізії %1…").arg(QString::fromStdString(oid).left(8)));

    auto thread = QThread::create([this, repoPath = repoPath_, oid, selection = fileSelection_,
                                   parseOpts = parseOptions_, jobId, token]() {
        using ParseRes = Result<code::ParsedSnapshot>;
        auto result = std::make_shared<ParseRes>(Cancelled{});

        try {
            JobContext ctx;
            ctx.jobId = jobId;
            ctx.stopToken = token;
            ctx.onProgress = [this, jobId](const Progress& p) {
                QMetaObject::invokeMethod(this, [this, jobId, p] {
                    if (jobId == currentJobId_ && !currentStopSource_.stop_requested()) {
                        emit progressChanged(p);
                    }
                }, Qt::QueuedConnection);
            };

            ctx.report("Завантаження дерева Git", 1, 2);
            history::RepositorySpec spec{repoPath};
            auto snapRes = history::loadSnapshot(spec, oid, selection, ctx);
            if (std::holds_alternative<Cancelled>(snapRes)) {
                *result = Cancelled{};
            } else if (std::holds_alternative<Error>(snapRes)) {
                *result = std::get<Error>(snapRes);
            } else {
                ctx.report("Розбір синтаксису коду", 2, 2);
                auto sourcePtr = std::make_shared<const code::SourceSnapshot>(
                    std::get<code::SourceSnapshot>(std::move(snapRes)));
                *result = code::parse(sourcePtr, parseOpts, ctx);
            }
        } catch (const std::exception& e) {
            *result = Error{ErrorCode::read_error, e.what()};
        } catch (...) {
            *result = Error{ErrorCode::read_error, "Невідома помилка завантаження знімку"};
        }

        QMetaObject::invokeMethod(this, [this, jobId, result]() {
            if (jobId != currentJobId_) {
                return;
            }
            if (currentStopSource_.stop_requested() || std::holds_alternative<Cancelled>(*result)) {
                emit operationCancelled(QString::fromUtf8("Завантаження знімку"));
                return;
            }
            if (std::holds_alternative<Error>(*result)) {
                const auto& err = std::get<Error>(*result);
                emit operationFailed(QString::fromUtf8("Завантаження знімку"), QString::fromStdString(err.message));
            } else if (auto* parsed = std::get_if<code::ParsedSnapshot>(result.get())) {
                emit commitSnapshotLoaded(std::make_shared<const code::ParsedSnapshot>(std::move(*parsed)));
                emit statusMessage(QString::fromUtf8("Знімок ревізії успішно завантажено"), 5000);
            }
        }, Qt::QueuedConnection);
    });

    thread->setParent(this);
    setWorker(thread, std::move(stopSource), jobId);
    thread->start();
}

void Coordinator::compareCommits(const std::string& baseOid, const std::string& targetOid) {
    if (repoPath_.empty()) {
        emit operationFailed(QString::fromUtf8("Порівняння комітів"), QString::fromUtf8("Репозиторій не вибрано"));
        return;
    }

    const auto jobId = ++currentJobId_;
    std::stop_source stopSource;
    const auto token = stopSource.get_token();

    emit statusMessage(QString::fromUtf8("Запуск ланцюжка Git → A → B → D для порівняння %1 ↔ %2…")
        .arg(QString::fromStdString(baseOid).left(7))
        .arg(QString::fromStdString(targetOid).left(7)));

    auto thread = QThread::create([this, repoPath = repoPath_, baseOid, targetOid,
                                   selection = fileSelection_, parseOpts = parseOptions_,
                                   profile = profile_, compOpts = comparisonOptions_,
                                   jobId, token]() {
        using CompRes = Result<review::ComparisonReport>;

        // Run the full pipeline and always return a result — never early-return
        // before invokeMethod, so errors and cancellations always reach the GUI.
        auto runPipeline = [&]() -> CompRes {
            JobContext ctx;
            ctx.jobId = jobId;
            ctx.stopToken = token;
            ctx.onProgress = [this, jobId](const Progress& p) {
                QMetaObject::invokeMethod(this, [this, jobId, p] {
                    if (jobId == currentJobId_ && !currentStopSource_.stop_requested()) {
                        emit progressChanged(p);
                    }
                }, Qt::QueuedConnection);
            };

            history::RepositorySpec spec{repoPath};

            // 1. Base snapshot
            ctx.report("Завантаження базового коміту", 1, 7);
            auto baseSnap = history::loadSnapshot(spec, baseOid, selection, ctx);
            if (std::holds_alternative<Cancelled>(baseSnap)) return Cancelled{};
            if (std::holds_alternative<Error>(baseSnap)) return std::get<Error>(baseSnap);

            // 2. Base parse
            ctx.report("Розбір коду базового коміту", 2, 7);
            auto baseSnapPtr = std::make_shared<const code::SourceSnapshot>(
                std::get<code::SourceSnapshot>(std::move(baseSnap)));
            auto baseParsed = code::parse(baseSnapPtr, parseOpts, ctx);
            if (std::holds_alternative<Cancelled>(baseParsed)) return Cancelled{};
            if (std::holds_alternative<Error>(baseParsed)) return std::get<Error>(baseParsed);

            // 3. Base analyze
            ctx.report("Аналіз метрик базового коміту", 3, 7);
            auto baseParsedPtr = std::make_shared<const code::ParsedSnapshot>(
                std::get<code::ParsedSnapshot>(std::move(baseParsed)));
            auto baseAnalysis = analysis::analyze(baseParsedPtr, profile, ctx);
            if (std::holds_alternative<Cancelled>(baseAnalysis)) return Cancelled{};
            if (std::holds_alternative<Error>(baseAnalysis)) return std::get<Error>(baseAnalysis);

            // 4. Target snapshot
            ctx.report("Завантаження цільового коміту", 4, 7);
            auto targetSnap = history::loadSnapshot(spec, targetOid, selection, ctx);
            if (std::holds_alternative<Cancelled>(targetSnap)) return Cancelled{};
            if (std::holds_alternative<Error>(targetSnap)) return std::get<Error>(targetSnap);

            // 5. Target parse
            ctx.report("Розбір коду цільового коміту", 5, 7);
            auto targetSnapPtr = std::make_shared<const code::SourceSnapshot>(
                std::get<code::SourceSnapshot>(std::move(targetSnap)));
            auto targetParsed = code::parse(targetSnapPtr, parseOpts, ctx);
            if (std::holds_alternative<Cancelled>(targetParsed)) return Cancelled{};
            if (std::holds_alternative<Error>(targetParsed)) return std::get<Error>(targetParsed);

            // 6. Target analyze
            ctx.report("Аналіз метрик цільового коміту", 6, 7);
            auto targetParsedPtr = std::make_shared<const code::ParsedSnapshot>(
                std::get<code::ParsedSnapshot>(std::move(targetParsed)));
            auto targetAnalysis = analysis::analyze(targetParsedPtr, profile, ctx);
            if (std::holds_alternative<Cancelled>(targetAnalysis)) return Cancelled{};
            if (std::holds_alternative<Error>(targetAnalysis)) return std::get<Error>(targetAnalysis);

            // 7. Review compare
            ctx.report("Порівняння ревізій та формування звіту", 7, 7);
            auto baseAnalysisPtr = std::make_shared<const analysis::AnalysisSnapshot>(
                std::get<analysis::AnalysisSnapshot>(std::move(baseAnalysis)));
            auto targetAnalysisPtr = std::make_shared<const analysis::AnalysisSnapshot>(
                std::get<analysis::AnalysisSnapshot>(std::move(targetAnalysis)));
            return review::compare(baseAnalysisPtr, targetAnalysisPtr, compOpts, ctx);
        };

        CompRes result = [&]() -> CompRes {
            try { return runPipeline(); }
            catch (const std::exception& e) { return Error{ErrorCode::invalid_input, e.what()}; }
            catch (...) { return Error{ErrorCode::invalid_input, "Невідома помилка порівняння комітів"}; }
        }();

        auto resultPtr = std::make_shared<CompRes>(std::move(result));
        QMetaObject::invokeMethod(this, [this, jobId, resultPtr]() {
            if (jobId != currentJobId_) {
                return;
            }
            if (currentStopSource_.stop_requested() || std::holds_alternative<Cancelled>(*resultPtr)) {
                emit operationCancelled(QString::fromUtf8("Порівняння комітів"));
                return;
            }
            if (std::holds_alternative<Error>(*resultPtr)) {
                const auto& err = std::get<Error>(*resultPtr);
                emit operationFailed(QString::fromUtf8("Порівняння комітів"), QString::fromStdString(err.message));
            } else if (auto* report = std::get_if<review::ComparisonReport>(resultPtr.get())) {
                emit comparisonCompleted(*report);
                emit statusMessage(QString::fromUtf8("Порівняння завершено успішно (знайдено %1 змін)")
                    .arg(report->items.size()), 5000);
            }
        }, Qt::QueuedConnection);
    });

    thread->setParent(this);
    setWorker(thread, std::move(stopSource), jobId);
    thread->start();
}

void Coordinator::buildTrend(const std::string& tipOid, std::size_t limit,
                             analysis::MetricId metricId, review::TrendAggregation agg,
                             const std::string& entityKey)
{
    if (repoPath_.empty()) {
        emit operationFailed(QString::fromUtf8("Побудова тренду"), QString::fromUtf8("Репозиторій не вибрано"));
        return;
    }
    if (tipOid.empty()) {
        emit operationFailed(QString::fromUtf8("Побудова тренду"), QString::fromUtf8("Не вказано початковий коміт (tip OID)"));
        return;
    }

    const auto jobId = ++currentJobId_;
    std::stop_source stopSource;
    const auto token = stopSource.get_token();

    emit statusMessage(QString::fromUtf8("Побудова тренду від коміту %1 (first-parent ланцюг)…")
        .arg(QString::fromStdString(tipOid).left(7)));

    auto thread = QThread::create([this, repoPath = repoPath_, tipOid, limit, metricId, agg,
                                   entityKey, selection = fileSelection_, parseOpts = parseOptions_,
                                   profile = profile_, jobId, token]() {
        using TrendRes = Result<review::TrendReport>;

        // Wrapped pipeline: always returns a result without early-returning from
        // the thread function, so the invokeMethod below always fires.
        auto runPipeline = [&]() -> TrendRes {
            JobContext ctx;
            ctx.jobId = jobId;
            ctx.stopToken = token;
            ctx.onProgress = [this, jobId](const Progress& p) {
                QMetaObject::invokeMethod(this, [this, jobId, p] {
                    if (jobId == currentJobId_ && !currentStopSource_.stop_requested()) {
                        emit progressChanged(p);
                    }
                }, Qt::QueuedConnection);
            };

            history::RepositorySpec spec{repoPath};
            history::HistoryRequest req;
            req.startOid = tipOid;
            req.limit = limit;
            req.parentIndex = 0;

            ctx.report("Отримання first-parent ланцюжка комітів", 0, limit);
            auto histRes = history::query(spec, req, ctx);
            if (std::holds_alternative<Cancelled>(histRes)) return Cancelled{};
            if (std::holds_alternative<Error>(histRes)) return std::get<Error>(histRes);

            const auto& historyResult = std::get<history::HistoryResult>(histRes);
            const auto& chain = historyResult.commits;
            if (chain.empty()) {
                return Error{ErrorCode::invalid_input, "Не знайдено комітів для побудови тренду від " + tipOid};
            }

            // Перевірка first-parent зв'язку
            std::vector<std::string> chainOids;
            chainOids.reserve(chain.size());
            for (const auto& c : chain) chainOids.push_back(c.oid);

            std::string validationError;
            if (!validateFirstParentChain(chain, chainOids, &validationError)) {
                return Error{ErrorCode::invalid_input, "Помилка валідації first-parent ланцюжка: " + validationError};
            }

            // revwalk дає від новіших до старіших; для OrderedAnalyses потрібні найстаріші першими
            std::vector<history::CommitRecord> orderedCommits(chain.rbegin(), chain.rend());

            review::OrderedAnalyses ordered;
            ordered.chainTipOid = tipOid;

            const std::size_t total = orderedCommits.size();
            for (std::size_t i = 0; i < total; ++i) {
                if (ctx.isCancelled()) return Cancelled{};
                const auto& c = orderedCommits[i];
                ctx.report("Аналіз ревізії " + c.oid.substr(0, 7), i + 1, total);

                auto snapRes = history::loadSnapshot(spec, c.oid, selection, ctx);
                if (std::holds_alternative<Cancelled>(snapRes)) return Cancelled{};
                if (std::holds_alternative<Error>(snapRes)) return std::get<Error>(snapRes);

                auto snapPtr = std::make_shared<const code::SourceSnapshot>(
                    std::get<code::SourceSnapshot>(std::move(snapRes)));
                auto parseRes = code::parse(snapPtr, parseOpts, ctx);
                if (std::holds_alternative<Cancelled>(parseRes)) return Cancelled{};
                if (std::holds_alternative<Error>(parseRes)) return std::get<Error>(parseRes);

                auto parsePtr = std::make_shared<const code::ParsedSnapshot>(
                    std::get<code::ParsedSnapshot>(std::move(parseRes)));
                auto anaRes = analysis::analyze(parsePtr, profile, ctx);
                if (std::holds_alternative<Cancelled>(anaRes)) return Cancelled{};
                if (std::holds_alternative<Error>(anaRes)) return std::get<Error>(anaRes);

                auto anaPtr = std::make_shared<const analysis::AnalysisSnapshot>(
                    std::get<analysis::AnalysisSnapshot>(std::move(anaRes)));
                ordered.snapshots.push_back(anaPtr);
            }

            review::TrendOptions trendOpts;
            trendOpts.metricId = metricId;
            trendOpts.aggregation = agg;
            trendOpts.entityKey = entityKey;
            // For file aggregations the selected target is a relative path.
            if (agg != review::TrendAggregation::function) {
                trendOpts.relativePath = entityKey;
            }
            trendOpts.limit = limit;

            // If aggregation is by function and no entityKey was specified, auto-detect
            // the first available function from the latest snapshot so the trend isn't empty gaps.
            if (trendOpts.aggregation == review::TrendAggregation::function && trendOpts.entityKey.empty()) {
                for (auto it = ordered.snapshots.rbegin(); it != ordered.snapshots.rend(); ++it) {
                    if (*it && (*it)->parsed) {
                        for (const auto& u : (*it)->parsed->units) {
                            for (const auto& fn : u.functions) {
                                if (fn && !fn->identityKey.empty()) {
                                    trendOpts.entityKey = fn->identityKey;
                                    break;
                                }
                            }
                            if (!trendOpts.entityKey.empty()) break;
                        }
                    }
                    if (!trendOpts.entityKey.empty()) break;
                }
            }

            ctx.report("Розрахунок тренду", total, total);
            return review::buildTrend(ordered, trendOpts, ctx);
        };

        TrendRes result = [&]() -> TrendRes {
            try { return runPipeline(); }
            catch (const std::exception& e) { return Error{ErrorCode::invalid_input, e.what()}; }
            catch (...) { return Error{ErrorCode::invalid_input, "Невідома помилка побудови тренду"}; }
        }();

        auto resultPtr = std::make_shared<TrendRes>(std::move(result));
        QMetaObject::invokeMethod(this, [this, jobId, resultPtr]() {
            if (jobId != currentJobId_) {
                return;
            }
            if (currentStopSource_.stop_requested() || std::holds_alternative<Cancelled>(*resultPtr)) {
                emit operationCancelled(QString::fromUtf8("Побудова тренду"));
                return;
            }
            if (std::holds_alternative<Error>(*resultPtr)) {
                const auto& err = std::get<Error>(*resultPtr);
                emit operationFailed(QString::fromUtf8("Побудова тренду"), QString::fromStdString(err.message));
            } else if (auto* report = std::get_if<review::TrendReport>(resultPtr.get())) {
                emit trendCompleted(*report);
                emit statusMessage(QString::fromUtf8("Тренд успішно побудовано (точок: %1)")
                    .arg(report->points.size()), 5000);
            }
        }, Qt::QueuedConnection);
    });

    thread->setParent(this);
    setWorker(thread, std::move(stopSource), jobId);
    thread->start();
}

} // namespace xray::app
