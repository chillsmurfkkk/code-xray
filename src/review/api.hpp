#pragma once

#include "common/job.hpp"
#include "review/types.hpp"

namespace xray::review {
Result<ComparisonReport> compare(std::shared_ptr<const analysis::AnalysisSnapshot> base,
                                 std::shared_ptr<const analysis::AnalysisSnapshot> target,
                                 const ComparisonOptions&, const JobContext&);
Result<TrendReport> buildTrend(const OrderedAnalyses&, const TrendOptions&, const JobContext&);
Result<ExportReceipt> exportReport(const ComparisonReport&, const ExportOptions&, const JobContext&);
} // namespace xray::review
