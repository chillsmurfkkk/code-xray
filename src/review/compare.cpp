// compare.cpp — D02, D01, D12, D13, D14, D15, D19, D25
// namespace xray::review
//
// Реалізує фасад compare() та всі допоміжні функції порівняння двох знімків аналізу.
// Зіставлення функцій і порушень делегується matchEntities/matchFindings з matching.hpp.
// Правила обходяться через ComparisonRule& (динамічний поліморфізм).

#include "review/api.hpp"
#include "review/matching.hpp"
#include "review/rules.hpp"

#include <algorithm>
#include <cassert>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace xray::review {

// ─────────────────────────────────────────────────────────────
// D02  validateCompatibility
// Перевіряємо сумісність двох знімків за версіями схем, профілем і правилами вибірки.
// Порожній відбиток відхиляється; різні відбитки — incompatible_analysis.
// ─────────────────────────────────────────────────────────────
static std::optional<Error> validateCompatibility(
    const analysis::AnalysisSnapshot& base,
    const analysis::AnalysisSnapshot& target)
{
    // Порожній відбиток несумісний зі всім
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
    // Перевіряємо однаковий профіль вибірки
    if (base.parsed && target.parsed) {
        if (base.parsed->source && target.parsed->source) {
            const auto& bSel = base.parsed->source->selection;
            const auto& tSel = target.parsed->source->selection;
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
// Зрозумілий текст для кожного виду зміни: ріст метрики, усунення, видалення, неоднозначність.
// ─────────────────────────────────────────────────────────────
static std::string makeReviewExplanation(
    ReviewKind kind,
    const std::string& entityKey,
    const std::vector<MetricDelta>& metrics,
    const std::vector<FindingChange>& findings)
{
    std::ostringstream out;

    switch (kind) {
    case ReviewKind::added:
        out << "Function '" << entityKey << "' was added in the target snapshot.";
        break;
    case ReviewKind::removed:
        out << "Function '" << entityKey << "' was removed in the target snapshot. "
            << "Any associated findings are marked as removed_with_code, not resolved.";
        break;
    case ReviewKind::unmatched:
        out << "Function '" << entityKey << "' could not be matched unambiguously "
            << "(duplicate key or incomplete analysis on one side). "
            << "Metric deltas and finding changes are not reported.";
        break;
    case ReviewKind::unchanged:
        out << "Function '" << entityKey << "' is present in both snapshots with no detected changes.";
        break;
    case ReviewKind::changed: {
        out << "Function '" << entityKey << "' changed between snapshots.";
        // Описуємо зміни метрик
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
        // Описуємо нові та усунені порушення
        int newCount = 0, resolvedCount = 0;
        for (const auto& f : findings) {
            if (f.state == FindingState::new_finding)  ++newCount;
            if (f.state == FindingState::resolved)      ++resolvedCount;
        }
        if (newCount > 0)     out << " " << newCount << " new finding(s).";
        if (resolvedCount > 0) out << " " << resolvedCount << " resolved finding(s).";
        break;
    }
    }
    return out.str();
}

// ─────────────────────────────────────────────────────────────
// D12  summarizeComparison
// Підраховує кількість елементів кожного виду без подвійного зарахування.
// Повертає рядок у форматі "added=N removed=N changed=N unchanged=N unmatched=N".
// (У повноцінному продукті це була б структура; для ЛР достатньо і методу.)
// ─────────────────────────────────────────────────────────────
std::string summarizeComparison(const std::vector<ReviewItem>& items)
{
    std::size_t added = 0, removed = 0, changed = 0, unchanged = 0, unmatched = 0;
    std::unordered_set<std::string> seen;
    for (const auto& item : items) {
        if (!seen.insert(item.id).second) continue; // не рахуємо двічі
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
// Групує елементи за відносним шляхом файлу (перший SourceRef), потім за видом зміни.
// Усередині групи: changed → added → removed → unmatched → unchanged.
// ─────────────────────────────────────────────────────────────
std::vector<ReviewItem> groupReviewItems(std::vector<ReviewItem> items)
{
    // Визначаємо порядок виду
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
            // Первинне сортування — за файлом
            const std::string& pathA = a.sourceRefs.empty() ? "" : a.sourceRefs.front().relativePath;
            const std::string& pathB = b.sourceRefs.empty() ? "" : b.sourceRefs.front().relativePath;
            if (pathA != pathB) return pathA < pathB;
            // Вторинне — за видом зміни
            int oa = kindOrder(a.kind), ob = kindOrder(b.kind);
            if (oa != ob) return oa < ob;
            // Третинне — за ключем функції
            return a.entityKey < b.entityKey;
        });

    return items;
}

// ─────────────────────────────────────────────────────────────
// D14  filterReviewItems
// Поєднаний фільтр: текст (entityKey), файл (relativePath), вид, прапор showUnknown.
// Порожній рядок фільтра = не фільтрувати за цим полем.
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
        // Фільтр тексту (підрядок у entityKey або explanation)
        if (!textFilter.empty()) {
            bool inKey  = item.entityKey.find(textFilter) != std::string::npos;
            bool inExpl = item.explanation.find(textFilter) != std::string::npos;
            if (!inKey && !inExpl) continue;
        }
        // Фільтр шляху
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
        // Фільтр виду
        if (kindFilter.has_value() && item.kind != *kindFilter) continue;
        // Прапор невідомих (confidence == incomplete ≈ unmatched)
        if (!showUnknown && item.confidence == Confidence::incomplete
            && item.kind == ReviewKind::unmatched) continue;

        result.push_back(item);
    }
    return result;
}

// ─────────────────────────────────────────────────────────────
// D15  rankReviewItems
// Сортує за вибраною метрикою (MetricId), спадання |delta|.
// nullopt дельта — завжди в кінці; рівні delta сортуються за entityKey.
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
            // nullopt завжди в кінці
            if (!da.has_value() && !db.has_value()) return a.entityKey < b.entityKey;
            if (!da.has_value()) return false;
            if (!db.has_value()) return true;
            // Спадання за |delta|
            long long absA = std::abs(static_cast<long long>(*da));
            long long absB = std::abs(static_cast<long long>(*db));
            if (absA != absB) return absA > absB;
            return a.entityKey < b.entityKey;
        });

    return items;
}

// ─────────────────────────────────────────────────────────────
// D25  validateComparisonReport
// Перевіряє: унікальність ID, узгодженість kind/confidence, відсутність порожніх ключів.
// Повертає список рядків з помилками (порожній = OK).
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
        // Видалений код не повинен мати delta
        if (item.kind == ReviewKind::removed) {
            for (const auto& m : item.metrics) {
                if (m.delta.has_value()) {
                    errors.push_back("ReviewItem '" + item.id + "' (removed) must not have metric delta.");
                }
            }
        }
        // unmatched → confidence повинна бути ambiguous або incomplete
        if (item.kind == ReviewKind::unmatched && item.confidence == Confidence::exact) {
            errors.push_back("ReviewItem '" + item.id + "' (unmatched) has Confidence::exact, which is contradictory.");
        }
    }
    return errors;
}

// ─────────────────────────────────────────────────────────────
// Helpers: побудова ReviewItem з EntityReview
// ─────────────────────────────────────────────────────────────
static ReviewKind matchKindToReviewKind(
    MatchKind mk,
    const std::vector<MetricDelta>& metrics,
    const std::vector<FindingChange>& findings)
{
    switch (mk) {
    case MatchKind::added:     return ReviewKind::added;
    case MatchKind::removed:   return ReviewKind::removed;
    case MatchKind::ambiguous: return ReviewKind::unmatched;
    case MatchKind::unmatched: return ReviewKind::unmatched;
    case MatchKind::matched: {
        // matched → changed якщо є delta або нові/усунені findings
        for (const auto& m : metrics) {
            if (m.delta.has_value() && *m.delta != 0) return ReviewKind::changed;
        }
        for (const auto& f : findings) {
            if (f.state == FindingState::new_finding || f.state == FindingState::resolved)
                return ReviewKind::changed;
        }
        return ReviewKind::unchanged;
    }
    }
    return ReviewKind::unmatched;
}

static Confidence matchKindToConfidence(MatchKind mk)
{
    switch (mk) {
    case MatchKind::matched:   return Confidence::exact;
    case MatchKind::added:     return Confidence::exact;
    case MatchKind::removed:   return Confidence::exact;
    case MatchKind::ambiguous: return Confidence::ambiguous;
    case MatchKind::unmatched: return Confidence::incomplete;
    }
    return Confidence::incomplete;
}

// ─────────────────────────────────────────────────────────────
// D01  compare — головний фасад
// Перевіряє сумісність (D02), зіставляє функції (D04 via matchEntities),
// порушення (D09–D11 via matchFindings), будує ComparisonReport з ReviewItem.
// ─────────────────────────────────────────────────────────────
Result<ComparisonReport> compare(
    std::shared_ptr<const analysis::AnalysisSnapshot> base,
    std::shared_ptr<const analysis::AnalysisSnapshot> target,
    const ComparisonOptions& options,
    const JobContext& ctx)
{
    // Перевірка вхідних даних
    if (!base || !target) {
        return Error{ErrorCode::invalid_input, "compare: base and target snapshots must not be null."};
    }

    ctx.report("Validating compatibility", 0, 3);

    // D02  validateCompatibility
    if (auto err = validateCompatibility(*base, *target)) {
        return *err;
    }

    if (ctx.isCancelled()) return Cancelled{};

    ctx.report("Matching entities", 1, 3);

    // Збираємо функції з parsed snapshot
    std::vector<const analysis::MetricResult*> baseMetrics, targetMetrics;
    std::vector<const analysis::Finding*>      baseFindings, targetFindings;

    // Індекс метрик за entityId — для швидкого пошуку
    std::unordered_map<std::string, std::vector<const analysis::MetricResult*>> baseMetricIdx, targetMetricIdx;
    for (const auto& m : base->metrics)   baseMetricIdx[m.entityId.empty() ? "" : m.entityId].push_back(&m);
    for (const auto& m : target->metrics) targetMetricIdx[m.entityId.empty() ? "" : m.entityId].push_back(&m);

    std::unordered_map<std::string, std::vector<const analysis::Finding*>> baseFindingIdx, targetFindingIdx;
    for (const auto& f : base->findings)   baseFindingIdx[f.entityId].push_back(&f);
    for (const auto& f : target->findings) targetFindingIdx[f.entityId].push_back(&f);

    // Збираємо всі функції з ParsedSnapshot
    struct FunctionRef {
        std::string identityKey;
        std::string entityId;
        std::string relativePath;
        code::SourceRange range;
        std::string sourceId;
    };

    auto collectFunctions = [](const analysis::AnalysisSnapshot& snap) -> std::vector<FunctionRef> {
        std::vector<FunctionRef> fns;
        if (!snap.parsed) return fns;
        for (const auto& unit : snap.parsed->units) {
            for (const auto& fn : unit.functions) {
                if (!fn) continue;
                fns.push_back({
                    fn->identityKey,
                    fn->id,
                    fn->relativePath,
                    fn->range,
                    snap.id
                });
            }
        }
        return fns;
    };

    auto baseFns   = collectFunctions(*base);
    auto targetFns = collectFunctions(*target);

    Completeness baseComp   = base->coverage.completeness;
    Completeness targetComp = target->coverage.completeness;

    // D04  matchEntities — зіставляємо за identityKey
    // (identityKey = ключ що будує A; ми тільки індексуємо)
    auto entityReviews = matchEntities(
        baseFns, targetFns,
        [](const FunctionRef& f) -> std::string { return f.identityKey; },
        // Метрика: беремо перше значення function_lines для цієї сутності
        [&](const FunctionRef& f) -> std::optional<std::int64_t> {
            const auto& idx = (&f >= baseFns.data() && &f < baseFns.data() + baseFns.size())
                              ? baseMetricIdx : targetMetricIdx;
            auto it = idx.find(f.entityId);
            if (it != idx.end()) {
                for (const auto* m : it->second) {
                    if (m->metricId == analysis::MetricId::function_lines && m->value.has_value())
                        return m->value;
                }
            }
            return std::nullopt;
        },
        baseComp, targetComp
    );

    if (ctx.isCancelled()) return Cancelled{};
    ctx.report("Building report", 2, 3);

    // Будуємо ReviewItem для кожного EntityReview
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

    // Накопичуємо всі діагностики
    for (const auto& e : base->coverage.diagnostics)   report.coverage.diagnostics.push_back(e);
    for (const auto& e : target->coverage.diagnostics) report.coverage.diagnostics.push_back(e);

    std::size_t itemIdx = 0;
    for (const auto& rev : entityReviews) {
        // Знаходимо FunctionRef для побудови SourceRef
        // (matched: обидва; added: тільки target; removed/unmatched: base)
        const FunctionRef* baseRef   = nullptr;
        const FunctionRef* targetRef = nullptr;

        // Підбираємо за ключем
        for (const auto& f : baseFns)   { if (f.identityKey == rev.key) { baseRef   = &f; break; } }
        for (const auto& f : targetFns) { if (f.identityKey == rev.key) { targetRef = &f; break; } }

        // Будуємо MetricDelta для всіх MetricId
        std::vector<MetricDelta> metricDeltas;
        for (auto metricId : {analysis::MetricId::function_lines,
                               analysis::MetricId::max_nesting,
                               analysis::MetricId::branch_count}) {
            MetricDelta md;
            md.metricId = metricId;

            auto findMetric = [&](const std::unordered_map<std::string, std::vector<const analysis::MetricResult*>>& idx,
                                   const FunctionRef* ref) -> std::optional<std::int64_t> {
                if (!ref) return std::nullopt;
                auto it = idx.find(ref->entityId);
                if (it == idx.end()) return std::nullopt;
                for (const auto* m : it->second) {
                    if (m->metricId == metricId && m->value.has_value()) return m->value;
                }
                return std::nullopt;
            };

            md.before = findMetric(baseMetricIdx, baseRef);
            md.after  = findMetric(targetMetricIdx, targetRef);
            if (md.before.has_value() && md.after.has_value()) {
                md.delta = *md.after - *md.before;
            }
            // Включаємо тільки якщо хоч одне значення є
            if (md.before.has_value() || md.after.has_value()) {
                metricDeltas.push_back(md);
            }
        }

        // Порушення для цієї функції
        std::vector<analysis::Finding> bFindings, tFindings;
        if (baseRef) {
            auto it = baseFindingIdx.find(baseRef->entityId);
            if (it != baseFindingIdx.end())
                for (const auto* f : it->second) bFindings.push_back(*f);
        }
        if (targetRef) {
            auto it = targetFindingIdx.find(targetRef->entityId);
            if (it != targetFindingIdx.end())
                for (const auto* f : it->second) tFindings.push_back(*f);
        }

        // D09/D10/D11  matchFindings
        auto findingChanges = matchFindings(
            rev.kind, bFindings, tFindings,
            [](const analysis::Finding& f) { return f.ruleId; },
            baseComp, targetComp
        );

        ReviewKind reviewKind = matchKindToReviewKind(rev.kind, metricDeltas, findingChanges);

        // Пропускаємо незмінені, якщо не потрібні
        if (!options.includeUnchanged && reviewKind == ReviewKind::unchanged) continue;

        // D19  makeReviewExplanation
        std::string explanation = makeReviewExplanation(reviewKind, rev.key, metricDeltas, findingChanges);

        // Будуємо SourceRef
        std::vector<SourceRef> sourceRefs;
        if (baseRef) {
            sourceRefs.push_back({baseRef->sourceId, baseRef->relativePath, {}, baseRef->range});
        }
        if (targetRef && targetRef != baseRef) {
            sourceRefs.push_back({targetRef->sourceId, targetRef->relativePath, {}, targetRef->range});
        }

        ReviewItem item;
        item.id         = report.baseId + ":" + report.targetId + ":" + rev.key;
        item.kind       = reviewKind;
        item.entityKey  = rev.key;
        item.before     = baseRef   ? std::optional<code::EntityId>{baseRef->entityId}   : std::nullopt;
        item.after      = targetRef ? std::optional<code::EntityId>{targetRef->entityId} : std::nullopt;
        item.explanation = std::move(explanation);
        item.confidence  = matchKindToConfidence(rev.kind);
        item.sourceRefs  = std::move(sourceRefs);
        item.metrics     = std::move(metricDeltas);
        item.findings    = std::move(findingChanges);

        report.items.push_back(std::move(item));
        ++itemIdx;
    }

    ctx.report("Done", 3, 3);
    return report;
}

} // namespace xray::review

