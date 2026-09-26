#pragma once

#include "analysis/types.hpp"
#include "common/job.hpp"

namespace xray::analysis {
Result<AnalysisSnapshot> analyze(std::shared_ptr<const code::ParsedSnapshot>,
                                 const AnalysisProfile&, const JobContext&);
} // namespace xray::analysis
