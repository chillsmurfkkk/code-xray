#include "analysis/metrics.hpp"

namespace xray::analysis {

namespace {

bool isNestingConstruct(code::ControlKind kind) noexcept {
    switch (kind) {
        case code::ControlKind::if_statement:
        case code::ControlKind::else_if:
        case code::ControlKind::for_loop:
        case code::ControlKind::range_for:
        case code::ControlKind::while_loop:
        case code::ControlKind::do_loop:
        case code::ControlKind::switch_statement:
        case code::ControlKind::try_statement:
        case code::ControlKind::catch_clause:
            return true;
        default:
            return false;
    }
}

void computeMaxNesting(
    const code::ControlNode& node,
    std::int64_t currentLevel,
    std::int64_t& maxLevel) {

    // Lambdas and local types are pruned per specification
    if (node.kind == code::ControlKind::lambda || node.kind == code::ControlKind::local_type) {
        return;
    }

    std::int64_t nextLevel = currentLevel;
    if (isNestingConstruct(node.kind)) {
        nextLevel = currentLevel + 1;
    }

    if (nextLevel > maxLevel) {
        maxLevel = nextLevel;
    }

    for (const auto& child : node.children) {
        computeMaxNesting(child, nextLevel, maxLevel);
    }
}

void computeBranchCount(const code::ControlNode& node, std::int64_t& count) {
    // Lambdas and local types are pruned per specification
    if (node.kind == code::ControlKind::lambda || node.kind == code::ControlKind::local_type) {
        return;
    }

    switch (node.kind) {
        case code::ControlKind::if_statement:
        case code::ControlKind::else_if:
        case code::ControlKind::for_loop:
        case code::ControlKind::range_for:
        case code::ControlKind::while_loop:
        case code::ControlKind::do_loop:
        case code::ControlKind::case_label:
        case code::ControlKind::catch_clause:
        case code::ControlKind::conditional_expression:
            ++count;
            break;
        case code::ControlKind::block:
        case code::ControlKind::switch_statement:
        case code::ControlKind::try_statement:
        case code::ControlKind::default_label:
        default:
            break;
    }

    for (const auto& child : node.children) {
        computeBranchCount(child, count);
    }
}

} // namespace

MetricResult LengthMetric::calculate(const code::FunctionEntity& function) const {
    MetricResult result;
    result.entityId = function.id;
    result.metricId = MetricId::function_lines;

    if (function.validity == code::Validity::unavailable) {
        result.value = std::nullopt;
        result.validity = code::Validity::unavailable;
        result.reason = function.reason.empty() ? "Unavailable due to parser errors" : function.reason;
        return result;
    }

    if (!function.bodyRange.has_value()) {
        result.value = std::nullopt;
        result.validity = code::Validity::not_applicable;
        result.reason = "Declaration without body";
        return result;
    }

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

MetricResult NestingMetric::calculate(const code::FunctionEntity& function) const {
    MetricResult result;
    result.entityId = function.id;
    result.metricId = MetricId::max_nesting;

    if (function.validity == code::Validity::unavailable) {
        result.value = std::nullopt;
        result.validity = code::Validity::unavailable;
        result.reason = function.reason.empty() ? "Unavailable due to parser errors" : function.reason;
        return result;
    }

    if (!function.bodyRange.has_value()) {
        result.value = std::nullopt;
        result.validity = code::Validity::not_applicable;
        result.reason = "Declaration without body";
        return result;
    }

    std::int64_t maxNesting = 0;
    // Walk controlTree starting from the function body root block
    for (const auto& child : function.controlTree.children) {
        computeMaxNesting(child, 0, maxNesting);
    }

    result.value = maxNesting;
    if (function.validity == code::Validity::syntactic_only) {
        result.validity = code::Validity::syntactic_only;
        result.reason = "Syntactic-only analysis";
    } else {
        result.validity = code::Validity::valid;
        result.reason.clear();
    }

    return result;
}

MetricResult BranchCountMetric::calculate(const code::FunctionEntity& function) const {
    MetricResult result;
    result.entityId = function.id;
    result.metricId = MetricId::branch_count;

    if (function.validity == code::Validity::unavailable) {
        result.value = std::nullopt;
        result.validity = code::Validity::unavailable;
        result.reason = function.reason.empty() ? "Unavailable due to parser errors" : function.reason;
        return result;
    }

    if (!function.bodyRange.has_value()) {
        result.value = std::nullopt;
        result.validity = code::Validity::not_applicable;
        result.reason = "Declaration without body";
        return result;
    }

    std::int64_t branches = 0;
    computeBranchCount(function.controlTree, branches);

    result.value = branches;
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
