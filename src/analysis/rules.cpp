#include "analysis/rules.hpp"

namespace xray::analysis {

std::string formatFindingExplanation(
    const code::FunctionEntity& function,
    std::string_view ruleName,
    std::int64_t observedValue,
    std::int64_t threshold,
    code::Validity validity) {
    std::string desc = "Rule '" + std::string(ruleName) + "': Function '" + function.describe() +
                       "' has " + std::to_string(observedValue) + " lines, exceeding threshold of " +
                       std::to_string(threshold) + " (" + function.relativePath + ":" +
                       std::to_string(function.range.startLine) + ").";
    if (validity == code::Validity::syntactic_only) {
        desc += " [Note: syntactic-only analysis]";
    }
    return desc;
}

std::optional<Finding> LongFunctionRule::evaluate(
    const code::FunctionEntity& function,
    const MetricResult& metric,
    const AnalysisProfile& profile) const {

    // Check if this rule is enabled in the profile
    const auto ruleIndex = static_cast<std::size_t>(RuleId::long_function);
    if (ruleIndex >= profile.enabledRules.size() || !profile.enabledRules[ruleIndex]) {
        return std::nullopt;
    }

    // Must match the target metric
    if (metric.metricId != MetricId::function_lines) {
        return std::nullopt;
    }

    // Must have a valid numeric value
    if (!metric.value.has_value() || metric.validity == code::Validity::unavailable ||
        metric.validity == code::Validity::not_applicable) {
        return std::nullopt;
    }

    const std::int64_t observed = *metric.value;
    const std::int64_t limit = profile.longFunctionLimit;

    // Requirement: strictly > threshold (equality is NOT a violation)
    if (observed <= limit) {
        return std::nullopt;
    }

    Finding finding;
    finding.id = "finding-" + function.id + "-long_function";
    finding.ruleId = RuleId::long_function;
    finding.entityId = function.id;
    finding.observedValue = observed;
    finding.threshold = limit;
    finding.validity = metric.validity;
    finding.explanation = formatFindingExplanation(function, name(), observed, limit, metric.validity);

    return finding;
}

} // namespace xray::analysis
