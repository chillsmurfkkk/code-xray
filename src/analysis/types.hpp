#pragma once

#include "code/types.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace xray::analysis {
enum class MetricId { function_lines, max_nesting, branch_count };
enum class RuleId { long_function, deep_nesting, many_branches };
struct AnalysisProfile {
    std::string id = "default-v1";
    std::uint32_t schemaVersion = 1;
    std::string name = "Default";
    std::int64_t longFunctionLimit = 60;
    std::int64_t nestingLimit = 3;
    std::int64_t branchLimit = 10;
    std::array<bool, 3> enabledRules{true, true, true}; // RuleId order.
};
struct MetricResult {
    code::EntityId entityId;
    MetricId metricId = MetricId::function_lines;
    std::optional<std::int64_t> value;
    code::Validity validity = code::Validity::unavailable;
    std::string reason;
};
struct Finding {
    std::string id;
    RuleId ruleId = RuleId::long_function;
    code::EntityId entityId;
    std::int64_t observedValue = 0;
    std::int64_t threshold = 0;
    code::Validity validity = code::Validity::valid;
    std::string explanation;
};
struct AnalysisSnapshot {
    std::string id;
    std::shared_ptr<const code::ParsedSnapshot> parsed;
    AnalysisProfile profile;
    std::string metricSchemaVersion = "metrics-v1";
    std::string analysisFingerprint;
    std::vector<MetricResult> metrics;
    std::vector<Finding> findings;
    Coverage coverage;
};
} // namespace xray::analysis
