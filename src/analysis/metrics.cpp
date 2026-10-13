#include "analysis/metrics.hpp"

namespace xray::analysis {

MetricResult LengthMetric::calculate(const code::FunctionEntity& function) const {
    MetricResult result;
    result.entityId = function.id;
    result.metricId = MetricId::function_lines;

    // 1. If function was marked unavailable due to parse errors, metrics cannot be computed.
    if (function.validity == code::Validity::unavailable) {
        result.value = std::nullopt;
        result.validity = code::Validity::unavailable;
        result.reason = function.reason.empty() ? "Unavailable due to parser errors" : function.reason;
        return result;
    }

    // 2. Declaration without a body (bodyRange is missing) is not applicable for body metrics.
    if (!function.bodyRange.has_value()) {
        result.value = std::nullopt;
        result.validity = code::Validity::not_applicable;
        result.reason = "Declaration without body";
        return result;
    }

    // 3. Physical line count: endLine - startLine + 1
    const auto startLine = static_cast<std::int64_t>(function.range.startLine);
    const auto endLine = static_cast<std::int64_t>(function.range.endLine);
    const std::int64_t lines = (endLine >= startLine) ? (endLine - startLine + 1) : 1;

    result.value = lines;
    if (function.validity == code::Validity::syntactic_only) {
        result.validity = code::Validity::syntactic_only;
        result.reason = "Syntactic-only analysis";
    } else {
        result.validity = code::Validity::valid;
        result.reason.clear();
    }

    return result;
}

} // namespace xray::analysis
