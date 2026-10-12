#pragma once

#include "review/types.hpp"
#include "common/job.hpp"
#include "common/result.hpp"

#include <vector>

namespace xray::review {

// D17 — extract a single trend point for a snapshot based on options
TrendPoint extractTrendPoint(const analysis::AnalysisSnapshot& snapshot, const TrendOptions& options);

// D18 — split trend points into continuous segments with non-null values
std::vector<std::vector<TrendPoint>> splitTrendSegments(const std::vector<TrendPoint>& points);

// D16 — build trend report across ordered snapshots
Result<TrendReport> buildTrend(const OrderedAnalyses& inputs, const TrendOptions& options, const JobContext& ctx);

} // namespace xray::review
