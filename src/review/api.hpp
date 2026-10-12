#pragma once

#include "common/job.hpp"
#include "review/types.hpp"
#include "analysis/types.hpp"
#include <optional>
#include <string>
#include <vector>

namespace xray::review {

// D01 — main comparison facade
Result<ComparisonReport> compare(std::shared_ptr<const analysis::AnalysisSnapshot> base,
                                 std::shared_ptr<const analysis::AnalysisSnapshot> target,
                                 const ComparisonOptions&, const JobContext&);

// D12 — count items per ReviewKind, no double-counting
std::string summarizeComparison(const std::vector<ReviewItem>& items);

// D13 — stable-sort by file path then kind then entityKey
std::vector<ReviewItem> groupReviewItems(std::vector<ReviewItem> items);

// D14 — combined filter: text, path, kind, showUnknown
std::vector<ReviewItem> filterReviewItems(
    const std::vector<ReviewItem>& items,
    const std::string& textFilter,
    const std::string& pathFilter,
    std::optional<ReviewKind> kindFilter,
    bool showUnknown);

// D15 — rank by |delta| of chosen metric, nullopt last
std::vector<ReviewItem> rankReviewItems(
    std::vector<ReviewItem> items,
    analysis::MetricId rankBy);

// D25 — validate report consistency; returns list of error strings (empty = OK)
std::vector<std::string> validateComparisonReport(const ComparisonReport& report);

// D16/D17/D18 — trend operations
Result<TrendReport> buildTrend(const OrderedAnalyses&, const TrendOptions&, const JobContext&);

// D20–D24 — export report facade
Result<ExportReceipt> exportReport(const ComparisonReport&, const ExportOptions&, const JobContext&);

} // namespace xray::review
