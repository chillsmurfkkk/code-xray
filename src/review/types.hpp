#pragma once

#include "analysis/types.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace xray::review {
struct ComparisonOptions { bool includeUnchanged = false; };
enum class ReviewKind { unchanged, changed, added, removed, unmatched };
enum class FindingState { new_finding, resolved, existing, removed_with_code, unknown };
enum class Confidence { exact, ambiguous, incomplete };
struct SourceRef {
    std::string sourceId;
    std::string relativePath;
    code::EntityId entityId;
    code::SourceRange range;
};
struct MetricDelta {
    analysis::MetricId metricId = analysis::MetricId::function_lines;
    std::optional<std::int64_t> before;
    std::optional<std::int64_t> after;
    std::optional<std::int64_t> delta;
};
struct FindingChange {
    analysis::RuleId ruleId = analysis::RuleId::long_function;
    FindingState state = FindingState::unknown;
};
struct ReviewItem {
    std::string id;
    ReviewKind kind = ReviewKind::unmatched;
    std::string entityKey;
    std::optional<code::EntityId> before;
    std::optional<code::EntityId> after;
    std::string explanation;
    Confidence confidence = Confidence::incomplete;
    std::vector<SourceRef> sourceRefs;
    std::vector<MetricDelta> metrics;
    std::vector<FindingChange> findings;
};
struct ComparisonReport {
    std::string baseId;
    std::string targetId;
    std::string analysisFingerprint;
    std::string metricSchemaVersion;
    std::shared_ptr<const analysis::AnalysisSnapshot> base;
    std::shared_ptr<const analysis::AnalysisSnapshot> target;
    ComparisonOptions options;
    std::vector<ReviewItem> items;
    Coverage coverage;
};

enum class TrendAggregation { function, file_mean, file_max };
struct TrendOptions {
    analysis::MetricId metricId = analysis::MetricId::function_lines;
    TrendAggregation aggregation = TrendAggregation::function;
    std::string entityKey;
    std::string relativePath;
    std::size_t limit = 20;
};
// app validates membership/order in ONE first-parent chain, including skipped commits.
struct OrderedAnalyses {
    std::string chainTipOid;
    std::vector<std::shared_ptr<const analysis::AnalysisSnapshot>> snapshots; // Oldest first.
};
struct TrendPoint {
    std::string sourceId;
    std::optional<double> value;
    std::size_t validFunctions = 0;
    std::size_t unknownFunctions = 0;
    std::string reason;
};
struct TrendReport {
    OrderedAnalyses inputs;
    TrendOptions options;
    std::string analysisFingerprint;
    std::vector<TrendPoint> points;
    Coverage coverage;
};
enum class ExportFormat { html, json };
struct ExportOptions {
    std::filesystem::path destination;
    ExportFormat format = ExportFormat::html;
    bool overwriteConfirmed = false;
};
struct ExportReceipt { std::filesystem::path path; std::uintmax_t bytesWritten = 0; };
} // namespace xray::review
