#pragma once
#include "analysis/types.hpp"
#include <algorithm>

namespace xray::analysis {
struct AggregateResult {
    std::optional<double> value;
    std::size_t included = 0, unknown = 0, notApplicable = 0, syntacticOnly = 0;
    std::vector<code::EntityId> maxima;
};
class MeanReducer {
public:
    void accumulate(const MetricResult& metric) {
        ++count_;
        // Online mean avoids overflowing an integer sum.
        mean_ += (static_cast<long double>(*metric.value) - mean_) / count_;
    }
    void finish(AggregateResult& result) const {
        if (count_) result.value = static_cast<double>(mean_);
    }
private:
    long double mean_ = 0;
    std::size_t count_ = 0;
};
class MaxReducer {
public:
    void accumulate(const MetricResult& metric) {
        if (!maximum_ || *metric.value > *maximum_) {
            maximum_ = metric.value;
            entities_ = {metric.entityId};
        } else if (*metric.value == *maximum_) {
            entities_.push_back(metric.entityId);
        }
    }
    void finish(AggregateResult& result) const {
        if (maximum_) result.value = static_cast<double>(*maximum_);
        result.maxima = entities_;
        std::sort(result.maxima.begin(), result.maxima.end());
    }
private:
    std::optional<std::int64_t> maximum_;
    std::vector<code::EntityId> entities_;
};
// Call with MeanReducer{} and MaxReducer{}: separate compile-time specializations.
// The input contains MetricResult records, including explicit unknown values.
template<class Reducer>
AggregateResult aggregateMetrics(const std::vector<MetricResult>& metrics, MetricId id, Reducer reducer) {
    AggregateResult result;
    for (const auto& metric : metrics) {
        if (metric.metricId != id) continue;
        if (metric.validity == code::Validity::not_applicable) { ++result.notApplicable; continue; }
        if (!metric.value || (metric.validity != code::Validity::valid &&
                              metric.validity != code::Validity::syntactic_only)) {
            ++result.unknown; continue;
        }
        reducer.accumulate(metric);
        ++result.included;
        if (metric.validity == code::Validity::syntactic_only) ++result.syntacticOnly;
    }
    reducer.finish(result);
    return result;
}
} // namespace xray::analysis
