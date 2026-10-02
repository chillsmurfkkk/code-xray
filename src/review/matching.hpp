#pragma once

#include "common/result.hpp"
#include <optional>
#include <cstddef>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include "review/types.hpp"

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
    ambiguous,
    unmatched
};
template <typename KeyType, typename MetricType>
struct EntityReview {
    KeyType key;
    MatchKind kind;
    std::optional<MetricType> before;
    std::optional<MetricType> after;
    std::optional<MetricType> metricDiff;
    std::vector<FindingChange> findings = {};
};

template <typename TEntity, typename KeyFn, typename MetricFn>
auto matchEntities(const std::vector<TEntity>& base,
                   const std::vector<TEntity>& target,
                   KeyFn keyFn,
                   MetricFn metricFn,
                   xray::Completeness baseCompleteness = xray::Completeness::complete,
                   xray::Completeness targetCompleteness = xray::Completeness::complete)
{
    using KeyType = std::invoke_result_t<KeyFn, const TEntity&>;
    using MetricType = std::invoke_result_t<MetricFn, const TEntity&>;

    std::vector<EntityReview<KeyType, MetricType>> results;
    auto joinRes = joinByKey(base, target, keyFn);

    for (const auto& pair : joinRes.matched) {
        auto before = metricFn(base[pair.leftIndex]);
        auto after = metricFn(target[pair.rightIndex]);
        results.push_back({keyFn(base[pair.leftIndex]), MatchKind::matched, before, after, after - before});
    }

    for (std::size_t idx : joinRes.unmatchedLeft) {
        auto before = metricFn(base[idx]);
        if (targetCompleteness == xray::Completeness::complete) {
            results.push_back({keyFn(base[idx]), MatchKind::removed, before, std::nullopt, std::nullopt});
        }
        else {
            results.push_back({keyFn(base[idx]), MatchKind::unmatched, before, std::nullopt, std::nullopt});
        }
    }

    for (std::size_t idx : joinRes.unmatchedRight) {
        auto after = metricFn(target[idx]);
        if (baseCompleteness == xray::Completeness::complete) {
            results.push_back({keyFn(target[idx]), MatchKind::added,std::nullopt, after, std::nullopt});
        }
        else {
            results.push_back({keyFn(target[idx]), MatchKind::unmatched,std::nullopt, after, std::nullopt});
        }
    }

    for (std::size_t idx : joinRes.ambiguousLeft) {
        results.push_back({keyFn(base[idx]), MatchKind::ambiguous,std::nullopt,std::nullopt,std::nullopt });
    }

    for (std::size_t idx : joinRes.ambiguousRight) {
        results.push_back({keyFn(target[idx]), MatchKind::ambiguous, std::nullopt,std::nullopt,std::nullopt});
    }

    return results;
}

   template <typename TFinding, typename RuleKeyFn>
std::vector<FindingChange> matchFindings(
    MatchKind entityKind,
    const std::vector<TFinding>& baseFindings,
    const std::vector<TFinding>& targetFindings,
    RuleKeyFn ruleKeyFn,
    xray::Completeness baseCompleteness = xray::Completeness::complete,
    xray::Completeness targetCompleteness = xray::Completeness::complete)
{
    std::vector<FindingChange> changes;

    if (entityKind == MatchKind::removed) {
        for (const auto& f : baseFindings) {
            changes.push_back({ruleKeyFn(f), FindingState::removed_with_code});
        }
        return changes;
    }

    if (entityKind == MatchKind::added) {
        for (const auto& f : targetFindings) {
            FindingState state = (baseCompleteness == xray::Completeness::complete)
                ? FindingState::new_finding
                : FindingState::unknown;
            changes.push_back({ruleKeyFn(f), state});
        }
        return changes;
    }

    if (entityKind == MatchKind::ambiguous || entityKind == MatchKind::unmatched) {
        for (const auto& f : baseFindings) {
            changes.push_back({ruleKeyFn(f), FindingState::unknown});
        }
        for (const auto& f : targetFindings) {
            changes.push_back({ruleKeyFn(f), FindingState::unknown});
        }
        return changes;
    }

    auto joinRes = joinByKey(baseFindings, targetFindings, ruleKeyFn);

    for (const auto& pair : joinRes.matched) {
        changes.push_back({ruleKeyFn(baseFindings[pair.leftIndex]), FindingState::existing});
    }

    for (std::size_t idx : joinRes.unmatchedLeft) {
        FindingState state = (targetCompleteness == xray::Completeness::complete)
            ? FindingState::resolved
            : FindingState::unknown;
        changes.push_back({ruleKeyFn(baseFindings[idx]), state});
    }

    for (std::size_t idx : joinRes.unmatchedRight) {
        FindingState state = (baseCompleteness == xray::Completeness::complete)
            ? FindingState::new_finding
            : FindingState::unknown;
        changes.push_back({ruleKeyFn(targetFindings[idx]), state});
    }

    for (std::size_t idx : joinRes.ambiguousLeft) {
        changes.push_back({ruleKeyFn(baseFindings[idx]), FindingState::unknown});
    }
    for (std::size_t idx : joinRes.ambiguousRight) {
        changes.push_back({ruleKeyFn(targetFindings[idx]), FindingState::unknown});
    }

    return changes;
}

} // namespace xray::review