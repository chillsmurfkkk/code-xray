// compare.cpp — D01, D02, D12, D13, D14, D15, D19, D25
// namespace xray::review

#include "review/api.hpp"
#include "review/matching.hpp"
#include "review/rules.hpp"

#include <algorithm>
#include <cassert>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace xray::review {

// ─────────────────────────────────────────────────────────────
// D02  validateCompatibility
// Порівнює параметри знімків, версії схем, профілі та канонічні фільтри.
// ─────────────────────────────────────────────────────────────
static std::optional<Error> validateCompatibility(
    const analysis::AnalysisSnapshot& base,
    const analysis::AnalysisSnapshot& target)
{
    if (base.analysisFingerprint.empty() || target.analysisFingerprint.empty()) {
        return Error{
            ErrorCode::incompatible_analysis,
            "One or both analysis snapshots have an empty fingerprint; "
            "comparison requires a non-empty fingerprint on both sides."
        };
    }
    if (base.analysisFingerprint != target.analysisFingerprint) {
        return Error{
            ErrorCode::incompatible_analysis,
            "Analysis fingerprints differ: base=\"" + base.analysisFingerprint +
            "\" target=\"" + target.analysisFingerprint + "\". "
            "Only snapshots with identical grammar/metric/profile parameters can be compared."
        };
    }
    if (base.metricSchemaVersion != target.metricSchemaVersion) {
        return Error{
            ErrorCode::incompatible_analysis,
            "Metric schema versions differ: base=\"" + base.metricSchemaVersion +
            "\" target=\"" + target.metricSchemaVersion + "\"."
        };
    }

    // Канонічне порівняння FileSelection: порядок елементів не має значення
    if (base.parsed && target.parsed) {
        if (base.parsed->source && target.parsed->source) {
            auto canonicalize = [](code::FileSelection sel) {
                std::sort(sel.extensions.begin(), sel.extensions.end());
                sel.extensions.erase(std::unique(sel.extensions.begin(), sel.extensions.end()), sel.extensions.end());
                std::sort(sel.excludedDirectories.begin(), sel.excludedDirectories.end());
                sel.excludedDirectories.erase(std::unique(sel.excludedDirectories.begin(), sel.excludedDirectories.end()), sel.excludedDirectories.end());
                std::sort(sel.relativePaths.begin(), sel.relativePaths.end());
                sel.relativePaths.erase(std::unique(sel.relativePaths.begin(), sel.relativePaths.end()), sel.relativePaths.end());
                return sel;
            };

            const auto bSel = canonicalize(base.parsed->source->selection);
            const auto tSel = canonicalize(target.parsed->source->selection);
            if (!(bSel == tSel)) {
                return Error{
                    ErrorCode::incompatible_analysis,
                    "File selection parameters differ between the two snapshots."
                };
            }
        }
    }
    return std::nullopt;
}

// ─────────────────────────────────────────────────────────────
// D19  makeReviewExplanation
// Зрозумілий текст для кожного виду зміни: приріст, усунення, видалення,
// неоднозначність, невідомі порушення та застереження syntactic_only.
// ─────────────────────────────────────────────────────────────
static std::string makeReviewExplanation(
    ReviewKind kind,
    const std::string& entityKey,
    const std::vector<MetricDelta>& metrics,
    const std::vector<FindingChange>& findings,
    Confidence confidence,
    bool isSyntacticOnly = false,
    const std::string& syntacticReason = "",
    bool hasUnavailableMetric = false)
{
    std::ostringstream out;

    switch (kind) {
    case ReviewKind::added:
        out << "Function '" << entityKey << "' was added in the target snapshot.";
        break;
    case ReviewKind::removed:
        out << "Function '" << entityKey << "' was removed in the target snapshot. "
            << "Associated findings are marked as removed_with_code, not resolved.";
        break;
    case ReviewKind::unmatched:
        if (confidence == Confidence::ambiguous) {
            out << "Function '" << entityKey << "' has ambiguous/duplicate identity key. "
                << "Automatic matching is disabled to prevent false comparisons.";
        } else {
            out << "Function '" << entityKey << "' could not be matched due to incomplete analysis "
                << "on one side. Metric deltas are withheld.";
        }
        break;
    case ReviewKind::unchanged:
        out << "Function '" << entityKey << "' is present in both snapshots with no detected changes.";
        break;
    case ReviewKind::changed: {
        out << "Function '" << entityKey << "' changed between snapshots.";
        for (const auto& m : metrics) {
            if (m.delta.has_value()) {
                const char* metricName = [&] {
                    switch (m.metricId) {
                    case analysis::MetricId::function_lines: return "lines";
                    case analysis::MetricId::max_nesting:    return "nesting depth";
                    case analysis::MetricId::branch_count:   return "branch count";
                    default:                                  return "metric";
                    }
                }();
                long long d = static_cast<long long>(*m.delta);
                out << " Metric '" << metricName << "': "
                    << (d > 0 ? "+" : "") << d
                    << " (" << (m.before.has_value() ? std::to_string(*m.before) : "?")
                    << " → " << (m.after.has_value()  ? std::to_string(*m.after)  : "?") << ").";
            }
        }
        int newCount = 0, resolvedCount = 0, unknownCount = 0;
        for (const auto& f : findings) {
            if (f.state == FindingState::new_finding)  ++newCount;
            if (f.state == FindingState::resolved)     ++resolvedCount;
            if (f.state == FindingState::unknown)      ++unknownCount;
        }
        if (newCount > 0)     out << " " << newCount << " new finding(s).";
        if (resolvedCount > 0) out << " " << resolvedCount << " resolved finding(s).";
        if (unknownCount > 0)  out << " " << unknownCount << " finding(s) in unknown state due to incomplete/unavailable analysis.";
        if (hasUnavailableMetric) {
            out << " One or more metrics are unavailable due to incomplete/unavailable analysis.";
        }
        break;
    }
    }

    // P2: Обов'язкове застереження про точність для syntactic_only
    if (isSyntacticOnly) {
        out << " Warning (syntactic_only): Analysis is purely syntactic and may be affected by conditional compilation or unexpanded preprocessor blocks.";
        if (!syntacticReason.empty()) {
            out << " [" << syntacticReason << "]";
        }
    }

    return out.str();
}

// ─────────────────────────────────────────────────────────────
// D12  summarizeComparison
// ─────────────────────────────────────────────────────────────
std::string summarizeComparison(const std::vector<ReviewItem>& items)
{
    std::size_t added = 0, removed = 0, changed = 0, unchanged = 0, unmatched = 0;
    std::unordered_set<std::string> seen;
    for (const auto& item : items) {
        if (!seen.insert(item.id).second) continue;
        switch (item.kind) {
        case ReviewKind::added:     ++added;     break;
        case ReviewKind::removed:   ++removed;   break;
        case ReviewKind::changed:   ++changed;   break;
        case ReviewKind::unchanged: ++unchanged; break;
        case ReviewKind::unmatched: ++unmatched; break;
        }
    }
    return "added=" + std::to_string(added)
         + " removed=" + std::to_string(removed)
         + " changed=" + std::to_string(changed)
         + " unchanged=" + std::to_string(unchanged)
         + " unmatched=" + std::to_string(unmatched);
}

// ─────────────────────────────────────────────────────────────
// D13  groupReviewItems
// ─────────────────────────────────────────────────────────────
std::vector<ReviewItem> groupReviewItems(std::vector<ReviewItem> items)
{
    auto kindOrder = [](ReviewKind k) -> int {
        switch (k) {
        case ReviewKind::changed:   return 0;
        case ReviewKind::added:     return 1;
        case ReviewKind::removed:   return 2;
        case ReviewKind::unmatched: return 3;
        case ReviewKind::unchanged: return 4;
        default:                    return 5;
        }
    };

    std::stable_sort(items.begin(), items.end(),
        [&](const ReviewItem& a, const ReviewItem& b) {
            const std::string& pathA = a.sourceRefs.empty() ? "" : a.sourceRefs.front().relativePath;
            const std::string& pathB = b.sourceRefs.empty() ? "" : b.sourceRefs.front().relativePath;
            if (pathA != pathB) return pathA < pathB;
            int oa = kindOrder(a.kind), ob = kindOrder(b.kind);
            if (oa != ob) return oa < ob;
            return a.entityKey < b.entityKey;
        });

    return items;
}

// ─────────────────────────────────────────────────────────────
// D14  filterReviewItems
// P2: showUnknown=false приховує всі невідомі, неповні та неоднозначні результати
// ─────────────────────────────────────────────────────────────
std::vector<ReviewItem> filterReviewItems(
    const std::vector<ReviewItem>& items,
    const std::string& textFilter,
    const std::string& pathFilter,
    std::optional<ReviewKind> kindFilter,
    bool showUnknown)
{
    std::vector<ReviewItem> result;
    result.reserve(items.size());

    for (const auto& item : items) {
        if (!textFilter.empty()) {
            bool inKey  = item.entityKey.find(textFilter) != std::string::npos;
            bool inExpl = item.explanation.find(textFilter) != std::string::npos;
            if (!inKey && !inExpl) continue;
        }
        if (!pathFilter.empty()) {
            bool inPath = false;
            for (const auto& ref : item.sourceRefs) {
                if (ref.relativePath.find(pathFilter) != std::string::npos) {
                    inPath = true;
                    break;
                }
            }
            if (!inPath) continue;
        }
        if (kindFilter.has_value() && item.kind != *kindFilter) continue;

        // P2: Повна фільтрація невідомих результатів при showUnknown == false
        if (!showUnknown) {
            // 1. Приховуємо unmatched
            if (item.kind == ReviewKind::unmatched) continue;
            // 2. Приховуємо будь-які елементи з incomplete або ambiguous confidence
            if (item.confidence == Confidence::incomplete || item.confidence == Confidence::ambiguous) continue;
            // 3. Приховуємо елементи з невідомими порушеннями
            bool hasUnknownFinding = false;
            for (const auto& f : item.findings) {
                if (f.state == FindingState::unknown) {
                    hasUnknownFinding = true;
                    break;
                }
            }
            if (hasUnknownFinding) continue;
            // 4. Приховуємо елементи з недоступними метриками (з урахуванням kind)
            bool hasUnavailableMetric = false;
            for (const auto& m : item.metrics) {
                if (item.kind == ReviewKind::changed || item.kind == ReviewKind::unchanged) {
                    if (!m.delta.has_value() || !m.before.has_value() || !m.after.has_value()) {
                        hasUnavailableMetric = true;
                        break;
                    }
                } else if (item.kind == ReviewKind::added) {
                    // Для доданих відсутність before та delta є очікуваною
                    if (!m.after.has_value()) {
                        hasUnavailableMetric = true;
                        break;
                    }
                } else if (item.kind == ReviewKind::removed) {
                    // Для видалених відсутність after та delta є очікуваною
                    if (!m.before.has_value()) {
                        hasUnavailableMetric = true;
                        break;
                    }
                }
            }
            if (hasUnavailableMetric) continue;
        }

        result.push_back(item);
    }
    return result;
}

// ─────────────────────────────────────────────────────────────
// D15  rankReviewItems
// ─────────────────────────────────────────────────────────────
std::vector<ReviewItem> rankReviewItems(
    std::vector<ReviewItem> items,
    analysis::MetricId rankBy)
{
    auto getDelta = [&](const ReviewItem& item) -> std::optional<std::int64_t> {
        for (const auto& m : item.metrics) {
            if (m.metricId == rankBy && m.delta.has_value()) return m.delta;
        }
        return std::nullopt;
    };

    std::stable_sort(items.begin(), items.end(),
        [&](const ReviewItem& a, const ReviewItem& b) {
            auto da = getDelta(a), db = getDelta(b);
            if (!da.has_value() && !db.has_value()) return a.entityKey < b.entityKey;
            if (!da.has_value()) return false;
            if (!db.has_value()) return true;
            long long absA = std::abs(static_cast<long long>(*da));
            long long absB = std::abs(static_cast<long long>(*db));
            if (absA != absB) return absA > absB;
            return a.entityKey < b.entityKey;
        });

    return items;
}

// ─────────────────────────────────────────────────────────────
// D25  validateComparisonReport
// ─────────────────────────────────────────────────────────────
std::vector<std::string> validateComparisonReport(const ComparisonReport& report)
{
    std::vector<std::string> errors;
    std::unordered_set<std::string> ids;

    for (const auto& item : report.items) {
        if (item.id.empty()) {
            errors.push_back("ReviewItem has empty id (entityKey='" + item.entityKey + "').");
        }
        if (!ids.insert(item.id).second) {
            errors.push_back("Duplicate ReviewItem id: '" + item.id + "'.");
        }
        if (item.entityKey.empty()) {
            errors.push_back("ReviewItem '" + item.id + "' has empty entityKey.");
        }

        if (item.kind == ReviewKind::changed || item.kind == ReviewKind::unchanged ||
            item.kind == ReviewKind::added || item.kind == ReviewKind::removed) {
            if (item.sourceRefs.empty()) {
                errors.push_back("ReviewItem '" + item.id + "' has empty sourceRefs.");
            }
        }
        for (const auto& ref : item.sourceRefs) {
            if (ref.sourceId.empty()) {
                errors.push_back("SourceRef in item '" + item.id + "' has empty sourceId.");
            }
            if (ref.relativePath.empty()) {
                errors.push_back("SourceRef in item '" + item.id + "' has empty relativePath.");
            }
            if (ref.entityId.empty()) {
                errors.push_back("SourceRef in item '" + item.id + "' has empty entityId.");
            }
            if (ref.range.startByte > ref.range.endByte) {
                errors.push_back("SourceRef in item '" + item.id + "' has invalid byte range.");
            }
        }

        if (item.kind == ReviewKind::removed || item.kind == ReviewKind::unmatched || item.kind == ReviewKind::added) {
            for (const auto& m : item.metrics) {
                if (m.delta.has_value()) {
                    errors.push_back("ReviewItem '" + item.id + "' of kind " +
                        std::to_string(static_cast<int>(item.kind)) + " must not have metric delta.");
                }
            }
        }

        if (item.kind == ReviewKind::unmatched && item.confidence == Confidence::exact) {
            errors.push_back("ReviewItem '" + item.id + "' (unmatched) cannot have Confidence::exact.");
        }
    }
    return errors;
}

// ─────────────────────────────────────────────────────────────
// D01  compare — головний фасад
// ─────────────────────────────────────────────────────────────
Result<ComparisonReport> compare(
    std::shared_ptr<const analysis::AnalysisSnapshot> base,
    std::shared_ptr<const analysis::AnalysisSnapshot> target,
    const ComparisonOptions& options,
    const JobContext& ctx)
{
    // P2: Валідація знімків — відсутні Parsed/SourceSnapshot не маскуються, а повертають invalid_input
    if (!base || !target) {
        return Error{ErrorCode::invalid_input, "compare: base and target snapshots must not be null."};
    }
    if (!base->parsed || !target->parsed) {
        return Error{ErrorCode::invalid_input, "compare: base and target snapshots must each contain a non-null ParsedSnapshot."};
    }
    if (!base->parsed->source || !target->parsed->source) {
        return Error{ErrorCode::invalid_input, "compare: base and target ParsedSnapshots must each contain a non-null SourceSnapshot."};
    }
    if (base->parsed->source->id.empty() || target->parsed->source->id.empty()) {
        return Error{ErrorCode::invalid_input, "compare: SourceSnapshot id cannot be empty."};
    }

    ctx.report("Validating compatibility", 0, 3);
    if (ctx.isCancelled()) return Cancelled{};

    if (auto err = validateCompatibility(*base, *target)) {
        return *err;
    }

    if (ctx.isCancelled()) return Cancelled{};
    ctx.report("Matching entities", 1, 3);

    std::unordered_map<std::string, std::vector<const analysis::MetricResult*>> baseMetricIdx, targetMetricIdx;
    for (const auto& m : base->metrics)   baseMetricIdx[m.entityId.empty() ? "" : m.entityId].push_back(&m);
    for (const auto& m : target->metrics) targetMetricIdx[m.entityId.empty() ? "" : m.entityId].push_back(&m);

    std::unordered_map<std::string, std::vector<const analysis::Finding*>> baseFindingIdx, targetFindingIdx;
    for (const auto& f : base->findings)   baseFindingIdx[f.entityId].push_back(&f);
    for (const auto& f : target->findings) targetFindingIdx[f.entityId].push_back(&f);

    struct FunctionRef {
        std::string identityKey;
        std::string entityId;
        std::string relativePath;
        code::SourceRange range;
        std::string sourceId;
        code::Validity validity = code::Validity::valid;
        std::string reason;
    };

    auto collectFunctions = [](const analysis::AnalysisSnapshot& snap) -> std::vector<FunctionRef> {
        std::vector<FunctionRef> fns;
        const std::string& sourceId = snap.parsed->source->id;
        for (const auto& unit : snap.parsed->units) {
            for (const auto& fn : unit.functions) {
                if (!fn) continue;
                fns.push_back({
                    fn->identityKey,
                    fn->id,
                    fn->relativePath,
                    fn->range,
                    sourceId,
                    fn->validity,
                    fn->reason
                });
            }
        }
        return fns;
    };

    auto baseFns   = collectFunctions(*base);
    auto targetFns = collectFunctions(*target);

    Completeness baseComp   = base->coverage.completeness;
    Completeness targetComp = target->coverage.completeness;

    auto joinRes = joinByKey(baseFns, targetFns, [](const FunctionRef& f) { return f.identityKey; });

    if (ctx.isCancelled()) return Cancelled{};
    ctx.report("Building report", 2, 3);

    std::vector<std::unique_ptr<ComparisonRule>> comparisonRules;
    comparisonRules.push_back(std::make_unique<MetricGrowthRule>(0));
    comparisonRules.push_back(std::make_unique<NewFindingRule>());
    comparisonRules.push_back(std::make_unique<ResolvedFindingRule>());

    ComparisonReport report;
    report.baseId                = base->id;
    report.targetId              = target->id;
    report.analysisFingerprint   = base->analysisFingerprint;
    report.metricSchemaVersion   = base->metricSchemaVersion;
    report.base                  = base;
    report.target                = target;
    report.options               = options;
    report.coverage.completeness = (baseComp == Completeness::complete && targetComp == Completeness::complete)
                                   ? Completeness::complete : Completeness::partial;
    report.coverage.skippedElements = base->coverage.skippedElements + target->coverage.skippedElements;

    for (const auto& e : base->coverage.diagnostics)   report.coverage.diagnostics.push_back(e);
    for (const auto& e : target->coverage.diagnostics) report.coverage.diagnostics.push_back(e);

    auto findMetricResult = [](const auto& idx, const FunctionRef* ref, analysis::MetricId mid)
        -> const analysis::MetricResult* {
        if (!ref) return nullptr;
        auto it = idx.find(ref->entityId);
        if (it == idx.end()) return nullptr;
        for (const auto* m : it->second) {
            if (m->metricId == mid) return m;
        }
        return nullptr;
    };

    auto getMetrics = [&](const FunctionRef* bRef, const FunctionRef* tRef, bool allowDelta,
                          bool& outHasUnavailable, bool& outIsSyntactic, std::string& outSyntacticReason) {
        std::vector<MetricDelta> deltas;
        for (auto metricId : {analysis::MetricId::function_lines,
                               analysis::MetricId::max_nesting,
                               analysis::MetricId::branch_count}) {
            MetricDelta md;
            md.metricId = metricId;

            const auto* bRes = findMetricResult(baseMetricIdx, bRef, metricId);
            const auto* tRes = findMetricResult(targetMetricIdx, tRef, metricId);

            md.before = bRes ? bRes->value : std::nullopt;
            md.after  = tRes ? tRes->value : std::nullopt;
            if (allowDelta && md.before.has_value() && md.after.has_value()) {
                md.delta = *md.after - *md.before;
            }

            bool bUnavailable = (bRes && bRes->validity == code::Validity::unavailable);
            bool tUnavailable = (tRes && tRes->validity == code::Validity::unavailable);
            if (bUnavailable || tUnavailable) {
                outHasUnavailable = true;
            }

            if ((bRes && bRes->validity == code::Validity::syntactic_only) ||
                (tRes && tRes->validity == code::Validity::syntactic_only)) {
                outIsSyntactic = true;
                if (outSyntacticReason.empty()) {
                    outSyntacticReason = (bRes && !bRes->reason.empty()) ? bRes->reason : (tRes ? tRes->reason : "");
                }
            }

            // P1: Зберігаємо метрику, якщо є число АБО якщо хоч один результат unavailable
            // (відрізняємо від not_applicable!)
            if (md.before.has_value() || md.after.has_value() || bUnavailable || tUnavailable) {
                deltas.push_back(md);
            }
        }
        return deltas;
    };

    auto getFindings = [&](const FunctionRef* ref, const auto& idx) {
        std::vector<analysis::Finding> findings;
        if (!ref) return findings;
        auto it = idx.find(ref->entityId);
        if (it != idx.end()) {
            for (const auto* f : it->second) findings.push_back(*f);
        }
        return findings;
    };

    // 1. Однозначно зіставлені функції (1-в-1)
    for (const auto& pair : joinRes.matched) {
        if (ctx.isCancelled()) return Cancelled{};

        const auto& bFn = baseFns[pair.leftIndex];
        const auto& tFn = targetFns[pair.rightIndex];

        bool hasUnavailableMetric = false;
        bool isSyntacticOnly = (bFn.validity == code::Validity::syntactic_only ||
                                tFn.validity == code::Validity::syntactic_only);
        std::string syntacticReason = !bFn.reason.empty() ? bFn.reason : tFn.reason;

        auto metricDeltas = getMetrics(&bFn, &tFn, /*allowDelta=*/true,
                                       hasUnavailableMetric, isSyntacticOnly, syntacticReason);
        auto bFindings = getFindings(&bFn, baseFindingIdx);
        auto tFindings = getFindings(&tFn, targetFindingIdx);

        auto findingChanges = matchFindings(
            MatchKind::matched, bFindings, tFindings,
            [](const analysis::Finding& f) { return f.ruleId; },
            baseComp, targetComp
        );

        EntityReview<std::string, std::int64_t> er{
            bFn.identityKey,
            MatchKind::matched,
            metricDeltas.empty() ? std::nullopt : metricDeltas.front().before,
            metricDeltas.empty() ? std::nullopt : metricDeltas.front().after,
            metricDeltas.empty() ? std::nullopt : metricDeltas.front().delta,
            findingChanges
        };
        bool ruleFlagged = false;
        for (const auto& rulePtr : comparisonRules) {
            const ComparisonRule& r = *rulePtr;
            if (r.evaluate(er)) {
                ruleFlagged = true;
            }
        }

        bool hasUnknownFinding = false;
        bool hasNewOrResolved = false;
        for (const auto& f : findingChanges) {
            if (f.state == FindingState::unknown) hasUnknownFinding = true;
            if (f.state == FindingState::new_finding || f.state == FindingState::resolved) hasNewOrResolved = true;
        }

        bool hasMetricDelta = false;
        bool hasPartialMetric = false;
        for (const auto& m : metricDeltas) {
            if (m.delta.has_value() && *m.delta != 0) hasMetricDelta = true;
            if (m.before.has_value() != m.after.has_value()) hasPartialMetric = true;
        }

        // P1: Недоступний результат або syntactic_only не видаються за «без змін»!
        ReviewKind rk = ReviewKind::unchanged;
        if (hasMetricDelta || hasPartialMetric || hasUnavailableMetric ||
            hasNewOrResolved || hasUnknownFinding || ruleFlagged || isSyntacticOnly) {
            rk = ReviewKind::changed;
        }

        if (!options.includeUnchanged && rk == ReviewKind::unchanged) {
            continue;
        }

        Confidence conf = (hasUnknownFinding || hasPartialMetric || hasUnavailableMetric || isSyntacticOnly)
            ? Confidence::incomplete : Confidence::exact;

        std::string explanation = makeReviewExplanation(
            rk, bFn.identityKey, metricDeltas, findingChanges, conf,
            isSyntacticOnly, syntacticReason, hasUnavailableMetric);

        ReviewItem item;
        item.id = report.baseId + ":" + report.targetId + ":" + bFn.entityId + "->" + tFn.entityId;
        item.kind = rk;
        item.entityKey = bFn.identityKey;
        item.before = bFn.entityId;
        item.after  = tFn.entityId;
        item.explanation = std::move(explanation);
        item.confidence = conf;
        item.sourceRefs.push_back({bFn.sourceId, bFn.relativePath, bFn.entityId, bFn.range});
        item.sourceRefs.push_back({tFn.sourceId, tFn.relativePath, tFn.entityId, tFn.range});
        item.metrics = std::move(metricDeltas);
        item.findings = std::move(findingChanges);

        report.items.push_back(std::move(item));
    }

    // 2. Видалені сутності
    for (std::size_t idx : joinRes.unmatchedLeft) {
        if (ctx.isCancelled()) return Cancelled{};

        const auto& bFn = baseFns[idx];
        bool hasUnavailable = false;
        bool isSyntactic = (bFn.validity == code::Validity::syntactic_only);
        std::string syntacticReason = bFn.reason;

        auto metricDeltas = getMetrics(&bFn, nullptr, /*allowDelta=*/false,
                                       hasUnavailable, isSyntactic, syntacticReason);
        auto bFindings = getFindings(&bFn, baseFindingIdx);

        MatchKind mk = (targetComp == Completeness::complete) ? MatchKind::removed : MatchKind::unmatched;
        auto findingChanges = matchFindings(
            mk, bFindings, {},
            [](const analysis::Finding& f) { return f.ruleId; },
            baseComp, targetComp
        );

        ReviewKind rk = (targetComp == Completeness::complete) ? ReviewKind::removed : ReviewKind::unmatched;
        Confidence conf = (targetComp == Completeness::complete && !isSyntactic)
            ? Confidence::exact : Confidence::incomplete;
        std::string explanation = makeReviewExplanation(
            rk, bFn.identityKey, metricDeltas, findingChanges, conf,
            isSyntactic, syntacticReason, hasUnavailable);

        ReviewItem item;
        item.id = report.baseId + ":" + report.targetId + ":del:" + bFn.entityId;
        item.kind = rk;
        item.entityKey = bFn.identityKey;
        item.before = bFn.entityId;
        item.after  = std::nullopt;
        item.explanation = std::move(explanation);
        item.confidence = conf;
        item.sourceRefs.push_back({bFn.sourceId, bFn.relativePath, bFn.entityId, bFn.range});
        item.metrics = std::move(metricDeltas);
        item.findings = std::move(findingChanges);

        report.items.push_back(std::move(item));
    }

    // 3. Додані сутності
    for (std::size_t idx : joinRes.unmatchedRight) {
        if (ctx.isCancelled()) return Cancelled{};

        const auto& tFn = targetFns[idx];
        bool hasUnavailable = false;
        bool isSyntactic = (tFn.validity == code::Validity::syntactic_only);
        std::string syntacticReason = tFn.reason;

        auto metricDeltas = getMetrics(nullptr, &tFn, /*allowDelta=*/false,
                                       hasUnavailable, isSyntactic, syntacticReason);
        auto tFindings = getFindings(&tFn, targetFindingIdx);

        MatchKind mk = (baseComp == Completeness::complete) ? MatchKind::added : MatchKind::unmatched;
        auto findingChanges = matchFindings(
            mk, {}, tFindings,
            [](const analysis::Finding& f) { return f.ruleId; },
            baseComp, targetComp
        );

        ReviewKind rk = (baseComp == Completeness::complete) ? ReviewKind::added : ReviewKind::unmatched;
        Confidence conf = (baseComp == Completeness::complete && !isSyntactic)
            ? Confidence::exact : Confidence::incomplete;
        std::string explanation = makeReviewExplanation(
            rk, tFn.identityKey, metricDeltas, findingChanges, conf,
            isSyntactic, syntacticReason, hasUnavailable);

        ReviewItem item;
        item.id = report.baseId + ":" + report.targetId + ":add:" + tFn.entityId;
        item.kind = rk;
        item.entityKey = tFn.identityKey;
        item.before = std::nullopt;
        item.after  = tFn.entityId;
        item.explanation = std::move(explanation);
        item.confidence = conf;
        item.sourceRefs.push_back({tFn.sourceId, tFn.relativePath, tFn.entityId, tFn.range});
        item.metrics = std::move(metricDeltas);
        item.findings = std::move(findingChanges);

        report.items.push_back(std::move(item));
    }

    // 4. Неоднозначні сутності з base
    for (std::size_t idx : joinRes.ambiguousLeft) {
        if (ctx.isCancelled()) return Cancelled{};

        const auto& bFn = baseFns[idx];
        bool hasUnavailable = false;
        bool isSyntactic = (bFn.validity == code::Validity::syntactic_only);
        std::string syntacticReason = bFn.reason;

        auto metricDeltas = getMetrics(&bFn, nullptr, /*allowDelta=*/false,
                                       hasUnavailable, isSyntactic, syntacticReason);
        auto bFindings = getFindings(&bFn, baseFindingIdx);

        std::vector<FindingChange> findingChanges;
        for (const auto& f : bFindings) {
            findingChanges.push_back({f.ruleId, FindingState::unknown});
        }

        std::string explanation = makeReviewExplanation(
            ReviewKind::unmatched, bFn.identityKey, metricDeltas, findingChanges, Confidence::ambiguous,
            isSyntactic, syntacticReason, hasUnavailable);

        ReviewItem item;
        item.id = report.baseId + ":" + report.targetId + ":ambig:base:" + bFn.entityId;
        item.kind = ReviewKind::unmatched;
        item.entityKey = bFn.identityKey;
        item.before = bFn.entityId;
        item.after  = std::nullopt;
        item.explanation = std::move(explanation);
        item.confidence = Confidence::ambiguous;
        item.sourceRefs.push_back({bFn.sourceId, bFn.relativePath, bFn.entityId, bFn.range});
        item.metrics = std::move(metricDeltas);
        item.findings = std::move(findingChanges);

        report.items.push_back(std::move(item));
    }

    // 5. Неоднозначні сутності з target
    for (std::size_t idx : joinRes.ambiguousRight) {
        if (ctx.isCancelled()) return Cancelled{};

        const auto& tFn = targetFns[idx];
        bool hasUnavailable = false;
        bool isSyntactic = (tFn.validity == code::Validity::syntactic_only);
        std::string syntacticReason = tFn.reason;

        auto metricDeltas = getMetrics(nullptr, &tFn, /*allowDelta=*/false,
                                       hasUnavailable, isSyntactic, syntacticReason);
        auto tFindings = getFindings(&tFn, targetFindingIdx);

        std::vector<FindingChange> findingChanges;
        for (const auto& f : tFindings) {
            findingChanges.push_back({f.ruleId, FindingState::unknown});
        }

        std::string explanation = makeReviewExplanation(
            ReviewKind::unmatched, tFn.identityKey, metricDeltas, findingChanges, Confidence::ambiguous,
            isSyntactic, syntacticReason, hasUnavailable);

        ReviewItem item;
        item.id = report.baseId + ":" + report.targetId + ":ambig:target:" + tFn.entityId;
        item.kind = ReviewKind::unmatched;
        item.entityKey = tFn.identityKey;
        item.before = std::nullopt;
        item.after  = tFn.entityId;
        item.explanation = std::move(explanation);
        item.confidence = Confidence::ambiguous;
        item.sourceRefs.push_back({tFn.sourceId, tFn.relativePath, tFn.entityId, tFn.range});
        item.metrics = std::move(metricDeltas);
        item.findings = std::move(findingChanges);

        report.items.push_back(std::move(item));
    }

    if (ctx.isCancelled()) return Cancelled{};
    ctx.report("Done", 3, 3);
    return report;
}

} // namespace xray::review
