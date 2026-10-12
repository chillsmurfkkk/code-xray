// trend.cpp — D16, D17, D18
#include "review/trend.hpp"

#include <algorithm>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace xray::review {

// ─────────────────────────────────────────────────────────────
// D17  extractTrendPoint
// Отримати значення функції/агрегату з обліком покриття й відсутності даних.
// ─────────────────────────────────────────────────────────────
TrendPoint extractTrendPoint(const analysis::AnalysisSnapshot& snapshot, const TrendOptions& options)
{
    TrendPoint pt;
    pt.sourceId = snapshot.id;

    // Швидкий індекс метрик за entityId та metricId
    std::unordered_map<std::string, const analysis::MetricResult*> metricMap;
    for (const auto& m : snapshot.metrics) {
        if (m.metricId == options.metricId) {
            metricMap[m.entityId] = &m;
        }
    }

    if (options.aggregation == TrendAggregation::function) {
        // Пошук конкретної функції
        const code::FunctionEntity* targetFn = nullptr;
        if (snapshot.parsed) {
            for (const auto& unit : snapshot.parsed->units) {
                if (!options.relativePath.empty() && unit.relativePath != options.relativePath) {
                    continue;
                }
                for (const auto& fn : unit.functions) {
                    if (fn && fn->identityKey == options.entityKey) {
                        targetFn = fn.get();
                        break;
                    }
                }
                if (targetFn) break;
            }
        }

        if (!targetFn) {
            pt.validFunctions = 0;
            pt.unknownFunctions = 1;
            pt.value = std::nullopt;
            pt.reason = "Function '" + options.entityKey + "' not found in snapshot";
            return pt;
        }

        auto it = metricMap.find(targetFn->id);
        if (it != metricMap.end() && it->second->value.has_value() &&
            it->second->validity == code::Validity::valid) {
            pt.validFunctions = 1;
            pt.unknownFunctions = 0;
            pt.value = static_cast<double>(*it->second->value);
            pt.reason = "Valid";
        } else {
            pt.validFunctions = 0;
            pt.unknownFunctions = 1;
            pt.value = std::nullopt;
            pt.reason = (it != metricMap.end() && !it->second->reason.empty())
                ? it->second->reason : "Metric unavailable for function";
        }
        return pt;
    }

    // Агрегація file_mean або file_max
    double sum = 0.0;
    double maxVal = std::numeric_limits<double>::lowest();
    std::size_t validCount = 0;
    std::size_t unknownCount = 0;

    if (snapshot.parsed) {
        for (const auto& unit : snapshot.parsed->units) {
            if (!options.relativePath.empty() && unit.relativePath != options.relativePath) {
                continue;
            }
            for (const auto& fn : unit.functions) {
                if (!fn) continue;
                auto it = metricMap.find(fn->id);
                if (it != metricMap.end() && it->second->value.has_value() &&
                    it->second->validity == code::Validity::valid) {
                    double val = static_cast<double>(*it->second->value);
                    sum += val;
                    if (val > maxVal) maxVal = val;
                    ++validCount;
                } else {
                    ++unknownCount;
                }
            }
        }
    }

    pt.validFunctions = validCount;
    pt.unknownFunctions = unknownCount;

    if (validCount > 0) {
        if (options.aggregation == TrendAggregation::file_mean) {
            pt.value = sum / static_cast<double>(validCount);
            pt.reason = "Mean of " + std::to_string(validCount) + " functions";
        } else if (options.aggregation == TrendAggregation::file_max) {
            pt.value = maxVal;
            pt.reason = "Max of " + std::to_string(validCount) + " functions";
        }
    } else {
        pt.value = std::nullopt;
        pt.reason = (unknownCount > 0)
            ? "No valid functions found (" + std::to_string(unknownCount) + " unknown)"
            : "No functions matched the filter criteria";
    }

    return pt;
}

// ─────────────────────────────────────────────────────────────
// D18  splitTrendSegments
// Поділити ряд на суцільні ділянки без домальовування ліній через невідомі точки.
// ─────────────────────────────────────────────────────────────
std::vector<std::vector<TrendPoint>> splitTrendSegments(const std::vector<TrendPoint>& points)
{
    std::vector<std::vector<TrendPoint>> segments;
    std::vector<TrendPoint> currentSegment;

    for (const auto& pt : points) {
        if (pt.value.has_value()) {
            currentSegment.push_back(pt);
        } else {
            if (!currentSegment.empty()) {
                segments.push_back(std::move(currentSegment));
                currentSegment.clear();
            }
        }
    }

    if (!currentSegment.empty()) {
        segments.push_back(std::move(currentSegment));
    }

    return segments;
}

// ─────────────────────────────────────────────────────────────
// D16  buildTrend
// Зібрати серію за впорядкованими знімками й перевірити сумісність аналізу.
// ─────────────────────────────────────────────────────────────
Result<TrendReport> buildTrend(
    const OrderedAnalyses& inputs,
    const TrendOptions& options,
    const JobContext& ctx)
{
    if (inputs.snapshots.empty()) {
        return Error{ErrorCode::invalid_input, "buildTrend: input snapshots list cannot be empty."};
    }

    // Перевірка сумісності відбитків
    const auto& firstSnap = inputs.snapshots.front();
    if (!firstSnap) {
        return Error{ErrorCode::invalid_input, "buildTrend: first snapshot is null."};
    }
    const std::string& expectedFp = firstSnap->analysisFingerprint;
    if (expectedFp.empty()) {
        return Error{ErrorCode::incompatible_analysis, "buildTrend: snapshot has empty analysis fingerprint."};
    }

    for (std::size_t i = 1; i < inputs.snapshots.size(); ++i) {
        const auto& s = inputs.snapshots[i];
        if (!s) {
            return Error{ErrorCode::invalid_input, "buildTrend: snapshot is null at index " + std::to_string(i)};
        }
        if (s->analysisFingerprint != expectedFp) {
            return Error{
                ErrorCode::incompatible_analysis,
                "buildTrend: snapshot fingerprint mismatch at index " + std::to_string(i) +
                " (expected '" + expectedFp + "', got '" + s->analysisFingerprint + "')"
            };
        }
    }

    ctx.report("Extracting trend points", 0, inputs.snapshots.size());

    TrendReport report;
    report.inputs = inputs;
    report.options = options;
    report.analysisFingerprint = expectedFp;
    report.coverage.completeness = Completeness::complete;

    // Обмеження кількості точок
    std::size_t count = inputs.snapshots.size();
    std::size_t startIdx = 0;
    if (options.limit > 0 && count > options.limit) {
        startIdx = count - options.limit;
    }

    for (std::size_t i = startIdx; i < count; ++i) {
        if (ctx.isCancelled()) return Cancelled{};

        const auto& snap = inputs.snapshots[i];
        report.points.push_back(extractTrendPoint(*snap, options));

        if (snap->coverage.completeness == Completeness::partial) {
            report.coverage.completeness = Completeness::partial;
        }

        ctx.report("Extracting trend points", i - startIdx + 1, count - startIdx);
    }

    return report;
}

} // namespace xray::review
