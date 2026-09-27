#pragma once
#include <string>

namespace xray::review {

class ComparisonRule {
public:
    virtual~ComparisonRule() = default;

    virtual std::string name() const = 0;

    virtual bool evaluate(const std::string& entityKey) const = 0;
};

class MetricGrowthRule : public ComparisonRule {
    int m_maxAllowedGrowth;
public:

    explicit MetricGrowthRule(int maxAllowedGrowth = 10)
    : m_maxAllowedGrowth(maxAllowedGrowth) {}

    std::string name() const override{
        return "MetricGrowthRule";
    }

    bool evaluate(const std::string& entityKey) const override {
        if (m_maxAllowedGrowth < entityKey.size()) {
            return false;
        }
        else {
            return true;
        }
    }
};
class NewFindingRule : public ComparisonRule {
        public:
        std::string name() const override {
            return "NewFindingRule";
        }

        bool evaluate(const std::string& entityKey) const override {
            return !entityKey.empty();
        }
    };

class ResolvedFindingRule : public ComparisonRule {
    public:

    std::string name() const override {
        return "ResolvedFindingRule";
    }

    bool evaluate(const std::string& entityKey) const override {
        return !entityKey.empty();
    }
};

} // namespace xray::review
