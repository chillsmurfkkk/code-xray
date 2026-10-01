#pragma once

#include <cstddef>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace xray::review {

template <typename TLeft, typename TRight>
struct JoinResult {
    struct IndexPair {
        std::size_t leftIndex;
        std::size_t rightIndex;
    };
    std::vector<IndexPair> matched;
    std::vector<std::size_t> unmatchedLeft;
    std::vector<std::size_t> unmatchedRight;
    std::vector<std::size_t> ambiguousLeft;
    std::vector<std::size_t> ambiguousRight;
};

template <typename TLeft, typename TRight, typename KeyFn>
JoinResult<TLeft, TRight> joinByKey(const std::vector<TLeft>& left,
                                    const std::vector<TRight>& right,
                                    KeyFn keyFn)
{
    JoinResult<TLeft, TRight> result;
    if (left.empty() && right.empty()) {
        return result;
    }

    using KeyType = std::invoke_result_t<KeyFn, const TLeft&>;

    std::unordered_map<KeyType, std::size_t> leftCounts;
    std::unordered_map<KeyType, std::size_t> rightCounts;

    for (const auto& l : left)  { leftCounts[keyFn(l)]++; }
    for (const auto& r : right) { rightCounts[keyFn(r)]++; }

    std::unordered_map<KeyType, std::size_t> uniqueRight;

    for (std::size_t j = 0; j < right.size(); ++j) {
        KeyType key = keyFn(right[j]);
        if (leftCounts[key] > 1 || rightCounts[key] > 1) {
            result.ambiguousRight.push_back(j);
        } else if (leftCounts[key] == 0) {
            result.unmatchedRight.push_back(j);
        } else {
            uniqueRight[key] = j;
        }
    }

    for (std::size_t k = 0; k < left.size(); ++k) {
        KeyType key = keyFn(left[k]);
        if (leftCounts[key] > 1 || rightCounts[key] > 1) {
            result.ambiguousLeft.push_back(k);
        } else if (rightCounts[key] == 1) {
            result.matched.push_back({k, uniqueRight[key]});
        } else {
            result.unmatchedLeft.push_back(k);
        }
    }

    return result;
}

enum class MatchKind : std::uint8_t {
    matched,
    added,
    removed,
    ambiguous
};
template <typename KeyType, typename MetricType>
struct EntityReview {
    KeyType key;
    MatchKind kind;
    MetricType metricDiff;
};

template <typename TEntity, typename KeyFn, typename MetricFn>
auto matchEntities(const std::vector<TEntity>& base,
                   const std::vector<TEntity>& target,
                   KeyFn keyFn,
                   MetricFn metricFn)
{
    using KeyType = std::invoke_result_t<KeyFn, const TEntity&>;
    using MetricType = std::invoke_result_t<MetricFn, const TEntity&>;

    std::vector<EntityReview<KeyType, MetricType>> results;
    auto joinRes = joinByKey(base, target, keyFn);

    for (const auto& pair : joinRes.matched) {
        auto before = metricFn(base[pair.leftIndex]);
        auto after = metricFn(target[pair.rightIndex]);
        results.push_back({keyFn(base[pair.leftIndex]), MatchKind::matched, after - before});
    }

    for (std::size_t idx : joinRes.unmatchedLeft) {
        auto before = metricFn(base[idx]);
        results.push_back({keyFn(base[idx]), MatchKind::removed, MetricType{} - before});
    }

    for (std::size_t idx : joinRes.unmatchedRight) {
        auto after = metricFn(target[idx]);
        results.push_back({keyFn(target[idx]), MatchKind::added, after});
    }

    for (std::size_t idx : joinRes.ambiguousLeft) {
        results.push_back({keyFn(base[idx]), MatchKind::ambiguous, MetricType{}});
    }

    for (std::size_t idx : joinRes.ambiguousRight) {
        results.push_back({keyFn(target[idx]), MatchKind::ambiguous, MetricType{}});
    }

    return results;
}

} // namespace xray::review