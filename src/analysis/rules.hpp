#pragma once

#include "analysis/types.hpp"
#include "code/types.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace xray::analysis {

// Abstract base class for all analysis rules (Dynamic Polymorphism 2)
class Rule {
public:
    virtual ~Rule() = default;

    [[nodiscard]] virtual RuleId id() const noexcept = 0;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] virtual MetricId targetMetricId() const noexcept = 0;
    [[nodiscard]] virtual std::optional<Finding> evaluate(
        const code::FunctionEntity& function,
        const MetricResult& metric,
        const AnalysisProfile& profile) const = 0;
};

// Long function rule: flags functions exceeding longFunctionLimit (strictly > threshold) (B04)
class LongFunctionRule final : public Rule {
public:
    [[nodiscard]] RuleId id() const noexcept override {
        return RuleId::long_function;
    }

    [[nodiscard]] std::string_view name() const noexcept override {
        return "long_function";
    }

    [[nodiscard]] MetricId targetMetricId() const noexcept override {
        return MetricId::function_lines;
    }

    [[nodiscard]] std::optional<Finding> evaluate(
        const code::FunctionEntity& function,
        const MetricResult& metric,
        const AnalysisProfile& profile) const override;
};

class DeepNestingRule final : public Rule {
public:
    [[nodiscard]] RuleId id() const noexcept override { return RuleId::deep_nesting; }
    [[nodiscard]] std::string_view name() const noexcept override { return "deep_nesting"; }
    [[nodiscard]] MetricId targetMetricId() const noexcept override { return MetricId::max_nesting; }
    [[nodiscard]] std::optional<Finding> evaluate(
        const code::FunctionEntity&, const MetricResult&, const AnalysisProfile&) const override;
};

class ManyBranchesRule final : public Rule {
public:
    [[nodiscard]] RuleId id() const noexcept override { return RuleId::many_branches; }
    [[nodiscard]] std::string_view name() const noexcept override { return "many_branches"; }
    [[nodiscard]] MetricId targetMetricId() const noexcept override { return MetricId::branch_count; }
    [[nodiscard]] std::optional<Finding> evaluate(
        const code::FunctionEntity&, const MetricResult&, const AnalysisProfile&) const override;
};

// Formats user-friendly explanation of a finding (B24)
std::string formatFindingExplanation(
    const code::FunctionEntity& function,
    std::string_view ruleName,
    std::int64_t observedValue,
    std::int64_t threshold,
    code::Validity validity);

} // namespace xray::analysis
