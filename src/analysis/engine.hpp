#pragma once
#include "analysis/api.hpp"
#include "analysis/metrics.hpp"
#include "analysis/rules.hpp"
#include <map>
#include <memory>

namespace xray::analysis {
using MetricRegistry = std::vector<std::unique_ptr<Metric>>;
using RuleRegistry = std::vector<std::unique_ptr<Rule>>;
Result<MetricRegistry> buildMetricRegistry();
Result<RuleRegistry> resolveEnabledRules(const AnalysisProfile&, const MetricRegistry&);
Result<std::string> makeAnalysisFingerprint(const code::ParsedSnapshot&, const AnalysisProfile&);
struct CoverageSummary {
    std::size_t valid = 0, syntacticOnly = 0, notApplicable = 0, unavailable = 0;
    Coverage coverage;
};
CoverageSummary summarizeCoverage(const code::ParsedSnapshot&);
struct FindingFilter {
    std::optional<RuleId> rule;
    std::string pathContains;
    std::string textContains;
    std::optional<code::Validity> validity;
};
std::vector<Finding> filterFindings(const AnalysisSnapshot&, const FindingFilter&);
std::map<std::string, std::vector<Finding>> groupFindingsByFile(const AnalysisSnapshot&);
std::vector<code::EntityId> rankFunctions(const AnalysisSnapshot&, MetricId, bool descending = true);
std::vector<Error> validateAnalysisResult(const AnalysisSnapshot&);
} // namespace xray::analysis
