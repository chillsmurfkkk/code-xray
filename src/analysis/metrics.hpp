#pragma once

#include "analysis/types.hpp"
#include "code/types.hpp"

#include <string_view>

namespace xray::analysis {

// Abstract base class for all code metrics (Dynamic Polymorphism 1)
class Metric {
public:
    virtual ~Metric() = default;

    [[nodiscard]] virtual MetricId id() const noexcept = 0;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] virtual MetricResult calculate(const code::FunctionEntity& function) const = 0;
};

// Length metric: calculates physical lines of a function (B01)
class LengthMetric final : public Metric {
public:
    [[nodiscard]] MetricId id() const noexcept override {
        return MetricId::function_lines;
    }

    [[nodiscard]] std::string_view name() const noexcept override {
        return "function_lines";
    }

    [[nodiscard]] MetricResult calculate(const code::FunctionEntity& function) const override;
};

// Nesting metric: calculates maximum nesting depth of control constructs (B02)
class NestingMetric final : public Metric {
public:
    [[nodiscard]] MetricId id() const noexcept override {
        return MetricId::max_nesting;
    }

    [[nodiscard]] std::string_view name() const noexcept override {
        return "max_nesting";
    }

    [[nodiscard]] MetricResult calculate(const code::FunctionEntity& function) const override;
};

// Branch count metric: calculates syntax branch points count (B03)
class BranchCountMetric final : public Metric {
public:
    [[nodiscard]] MetricId id() const noexcept override {
        return MetricId::branch_count;
    }

    [[nodiscard]] std::string_view name() const noexcept override {
        return "branch_count";
    }

    [[nodiscard]] MetricResult calculate(const code::FunctionEntity& function) const override;
};

} // namespace xray::analysis
