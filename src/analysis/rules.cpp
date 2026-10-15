#include "analysis/rules.hpp"

namespace xray::analysis {

std::string formatFindingExplanation(
    const code::FunctionEntity& function, std::string_view ruleName,
    std::int64_t observedValue, std::int64_t threshold, code::Validity validity) {
    const auto unit = ruleName == "long_function" ? "lines" :
                      ruleName == "deep_nesting" ? "nesting levels" : "branches";
    std::string desc = "Rule '" + std::string(ruleName) + "': Function '" + function.describe() +
        "' has " + std::to_string(observedValue) + " " + unit + ", exceeding threshold of " +
        std::to_string(threshold) + " (" + function.relativePath + ":" +
        std::to_string(function.range.startLine) + ").";
    if (validity == code::Validity::syntactic_only) {
        desc += " [Note: syntactic-only analysis]";
    }
    return desc;
}

namespace {
std::optional<Finding> evaluateThreshold(
    const Rule& rule, const code::FunctionEntity& function,
    const MetricResult& metric, const AnalysisProfile& profile, std::int64_t limit) {
    const auto index = static_cast<std::size_t>(rule.id());
    if (!profile.enabledRules[index] || limit < 0 || metric.entityId != function.id ||
        metric.metricId != rule.targetMetricId() || !function.bodyRange ||
        function.validity == code::Validity::unavailable ||
        function.validity == code::Validity::not_applicable || !metric.value ||
        (metric.validity != code::Validity::valid && metric.validity != code::Validity::syntactic_only) ||
        *metric.value <= limit) {
        return std::nullopt;
    }
    Finding finding;
    finding.id = "finding-" + function.id + "-" + std::string(rule.name());
    finding.ruleId = rule.id();
    finding.entityId = function.id;
    finding.observedValue = *metric.value;
    finding.threshold = limit;
    finding.validity = (metric.validity == code::Validity::syntactic_only ||
                        function.validity == code::Validity::syntactic_only)
        ? code::Validity::syntactic_only : code::Validity::valid;
    finding.explanation = formatFindingExplanation(
        function, rule.name(), *metric.value, limit, finding.validity);
    return finding;
}
} // namespace

std::optional<Finding> LongFunctionRule::evaluate(
    const code::FunctionEntity& fn, const MetricResult& metric, const AnalysisProfile& profile) const {
    return evaluateThreshold(*this, fn, metric, profile, profile.longFunctionLimit);
}
std::optional<Finding> DeepNestingRule::evaluate(
    const code::FunctionEntity& fn, const MetricResult& metric, const AnalysisProfile& profile) const {
    return evaluateThreshold(*this, fn, metric, profile, profile.nestingLimit);
}
std::optional<Finding> ManyBranchesRule::evaluate(
    const code::FunctionEntity& fn, const MetricResult& metric, const AnalysisProfile& profile) const {
    return evaluateThreshold(*this, fn, metric, profile, profile.branchLimit);
}
} // namespace xray::analysis
