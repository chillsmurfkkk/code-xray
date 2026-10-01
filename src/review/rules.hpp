#pragma once

#include "review/types.hpp"
#include "review/matching.hpp"
#include <string>
#include <cstdint>

namespace xray::review {

class ComparisonRule {
public:
    virtual ~ComparisonRule() = default;

    virtual std::string name() const = 0;

    virtual bool evaluate(const EntityReview<std::string, std::int64_t>& review) const = 0;
};

class MetricGrowthRule : public ComparisonRule {
    std::int64_t m_maxAllowedGrowth;
public:
    explicit MetricGrowthRule(std::int64_t maxAllowedGrowth = 10)
    : m_maxAllowedGrowth(maxAllowedGrowth) {}

    std::string name() const override {
        return "MetricGrowthRule";
    }

    bool evaluate(const EntityReview<std::string, std::int64_t>& review) const override {
        return review.metricDiff > m_maxAllowedGrowth;
    }
};

class NewFindingRule : public ComparisonRule {
public:
    std::string name() const override {
        return "NewFindingRule";
    }

    bool evaluate(const EntityReview<std::string, std::int64_t>& review) const override {
        return review.kind == MatchKind::added;
    }
};

class ResolvedFindingRule : public ComparisonRule {
public:
    std::string name() const override {
        return "ResolvedFindingRule";
    }

    bool evaluate(const EntityReview<std::string, std::int64_t>& review) const override {
        return review.kind == MatchKind::removed;
    }
};

} // namespace xray::review