#include "review/matching.hpp"
#include "review/rules.hpp"
#include "review/exporter.hpp"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <optional>

int main() {
    using namespace xray;
    using namespace xray::review;

    struct DummyEntity {
        std::string name;
        std::int64_t metric;
    };

    // 1. matchEntities: повнота аналізу та знакова різниця
    std::vector<DummyEntity> base = {
        {"func_match", 100},
        {"func_remove", 50}
    };
    std::vector<DummyEntity> target = {
        {"func_match", 120},
        {"func_add", 30}
    };

    auto resultsComplete = matchEntities(
        base, target,
        [](const DummyEntity& e) { return e.name; },
        [](const DummyEntity& e) { return e.metric; },
        Completeness::complete, Completeness::complete
    );

    assert(resultsComplete.size() == 3);

    auto findByKey = [](const auto& list, const std::string& key) -> const EntityReview<std::string, std::int64_t>* {
        for (const auto& r : list) {
            if (r.key == key) return &r;
        }
        return nullptr;
    };

    auto* revMatch = findByKey(resultsComplete, "func_match");
    assert(revMatch != nullptr);
    assert(revMatch->kind == MatchKind::matched);
    assert(revMatch->before == 100);
    assert(revMatch->after == 120);
    assert(revMatch->metricDiff.has_value());
    assert(*revMatch->metricDiff == 20);

    auto* revRemove = findByKey(resultsComplete, "func_remove");
    assert(revRemove != nullptr);
    assert(revRemove->kind == MatchKind::removed);
    assert(revRemove->before == 50);
    assert(!revRemove->after.has_value());
    assert(!revRemove->metricDiff.has_value());

    auto* revAdd = findByKey(resultsComplete, "func_add");
    assert(revAdd != nullptr);
    assert(revAdd->kind == MatchKind::added);
    assert(!revAdd->before.has_value());
    assert(revAdd->after == 30);
    assert(!revAdd->metricDiff.has_value());

    // 1.1 Підтримка MetricResult::value (std::optional<std::int64_t>) від B:
    // Функцію зіставлено, але одне або обидва значення недоступні -> delta = nullopt
    struct EntityWithOptionalMetric {
        std::string name;
        std::optional<std::int64_t> metric;
    };

    std::vector<EntityWithOptionalMetric> baseOpt = {
        {"func_both_valid", 40},
        {"func_target_unavailable", 50},
        {"func_base_unavailable", std::nullopt}
    };
    std::vector<EntityWithOptionalMetric> targetOpt = {
        {"func_both_valid", 70},
        {"func_target_unavailable", std::nullopt},
        {"func_base_unavailable", 30}
    };

    auto resultsOpt = matchEntities(
        baseOpt, targetOpt,
        [](const EntityWithOptionalMetric& e) { return e.name; },
        [](const EntityWithOptionalMetric& e) { return e.metric; }
    );

    auto* revBothValid = findByKey(resultsOpt, "func_both_valid");
    assert(revBothValid != nullptr);
    assert(revBothValid->kind == MatchKind::matched);
    assert(revBothValid->metricDiff.has_value());
    assert(*revBothValid->metricDiff == 30); // 70 - 40

    auto* revTargetUnavail = findByKey(resultsOpt, "func_target_unavailable");
    assert(revTargetUnavail != nullptr);
    assert(revTargetUnavail->kind == MatchKind::matched);
    assert(revTargetUnavail->before == 50);
    assert(!revTargetUnavail->after.has_value());
    assert(!revTargetUnavail->metricDiff.has_value()); // nullopt, бо after недоступний!

    auto* revBaseUnavail = findByKey(resultsOpt, "func_base_unavailable");
    assert(revBaseUnavail != nullptr);
    assert(revBaseUnavail->kind == MatchKind::matched);
    assert(!revBaseUnavail->before.has_value());
    assert(revBaseUnavail->after == 30);
    assert(!revBaseUnavail->metricDiff.has_value()); // nullopt, бо before недоступний!

    // 2. Неповний аналіз дає unmatched
    auto resultsPartialTarget = matchEntities(
        base, target,
        [](const DummyEntity& e) { return e.name; },
        [](const DummyEntity& e) { return e.metric; },
        Completeness::complete, Completeness::partial
    );
    auto* revRemovePartial = findByKey(resultsPartialTarget, "func_remove");
    assert(revRemovePartial != nullptr);
    assert(revRemovePartial->kind == MatchKind::unmatched);
    assert(!revRemovePartial->metricDiff.has_value());

    auto resultsPartialBase = matchEntities(
        base, target,
        [](const DummyEntity& e) { return e.name; },
        [](const DummyEntity& e) { return e.metric; },
        Completeness::partial, Completeness::complete
    );
    auto* revAddPartial = findByKey(resultsPartialBase, "func_add");
    assert(revAddPartial != nullptr);
    assert(revAddPartial->kind == MatchKind::unmatched);
    assert(!revAddPartial->metricDiff.has_value());

    // 3. Дублікати ключів
    std::vector<DummyEntity> baseDup = {
        {"dup_key", 10},
        {"dup_key", 20},
        {"unique", 30}
    };
    std::vector<DummyEntity> targetDup = {
        {"dup_key", 15},
        {"unique", 40}
    };

    auto dupResults = matchEntities(
        baseDup, targetDup,
        [](const DummyEntity& e) { return e.name; },
        [](const DummyEntity& e) { return e.metric; }
    );

    std::size_t ambiguousCount = 0;
    std::size_t matchedCount = 0;
    for (const auto& r : dupResults) {
        if (r.key == "dup_key") {
            assert(r.kind == MatchKind::ambiguous);
            assert(!r.metricDiff.has_value());
            ++ambiguousCount;
        }
        if (r.key == "unique") {
            assert(r.kind == MatchKind::matched);
            assert(r.metricDiff.has_value());
            assert(*r.metricDiff == 10);
            ++matchedCount;
        }
    }
    assert(ambiguousCount == 3);
    assert(matchedCount == 1);

    // 4. matchFindings: розрізнення resolved vs removed_with_code vs new_finding
    struct DummyFinding {
        analysis::RuleId ruleId;
    };

    std::vector<DummyFinding> oldFindings = {{analysis::RuleId::long_function}};
    std::vector<DummyFinding> noFindings = {};

    auto removedFindings = matchFindings(
        MatchKind::removed,
        oldFindings, noFindings,
        [](const DummyFinding& f) { return f.ruleId; }
    );
    assert(removedFindings.size() == 1);
    assert(removedFindings.front().state == FindingState::removed_with_code);

    auto resolvedFindings = matchFindings(
        MatchKind::matched,
        oldFindings, noFindings,
        [](const DummyFinding& f) { return f.ruleId; }
    );
    assert(resolvedFindings.size() == 1);
    assert(resolvedFindings.front().state == FindingState::resolved);

    std::vector<DummyFinding> newFindingsList = {{analysis::RuleId::deep_nesting}};
    auto newFindings = matchFindings(
        MatchKind::matched,
        noFindings, newFindingsList,
        [](const DummyFinding& f) { return f.ruleId; }
    );
    assert(newFindings.size() == 1);
    assert(newFindings.front().state == FindingState::new_finding);

    // 5. Правила
    MetricGrowthRule growthRule(10);
    NewFindingRule newRule;
    ResolvedFindingRule resolvedRule;

    assert(resolvedRule.evaluateFinding(FindingState::resolved) == true);
    assert(resolvedRule.evaluateFinding(FindingState::removed_with_code) == false);
    assert(resolvedRule.evaluateFinding(FindingState::new_finding) == false);

    assert(newRule.evaluateFinding(FindingState::new_finding) == true);
    assert(newRule.evaluateFinding(FindingState::removed_with_code) == false);
    assert(newRule.evaluateFinding(FindingState::resolved) == false);

    EntityReview<std::string, std::int64_t> revWithResolved{
        "f_resolved", MatchKind::matched, 10, 10, 0,
        {{analysis::RuleId::long_function, FindingState::resolved}}
    };
    EntityReview<std::string, std::int64_t> revWithRemovedCode{
        "f_deleted", MatchKind::removed, 50, std::nullopt, std::nullopt,
        {{analysis::RuleId::long_function, FindingState::removed_with_code}}
    };
    EntityReview<std::string, std::int64_t> revWithNew{
        "f_new_finding", MatchKind::matched, 10, 10, 0,
        {{analysis::RuleId::deep_nesting, FindingState::new_finding}}
    };

    assert(resolvedRule.evaluate(revWithResolved) == true);
    assert(resolvedRule.evaluate(revWithRemovedCode) == false);
    assert(resolvedRule.evaluate(revWithNew) == false);

    assert(newRule.evaluate(revWithNew) == true);
    assert(newRule.evaluate(revWithRemovedCode) == false);
    assert(newRule.evaluate(revWithResolved) == false);

    ComparisonRule* rule = &resolvedRule;
    assert(rule->name() == "ResolvedFindingRule");
    assert(rule->evaluate(revWithResolved) == true);
    assert(rule->evaluate(revWithRemovedCode) == false);

    // 6. Експортери (заглушки першого PR повертають false, оскільки реальний експорт ще не реалізовано)
    HtmlExporter htmlExporter;
    JsonExporter jsonExporter;
    ExportOptions opts;
    opts.destination = "report.html";

    ReportExporter* exporter = &htmlExporter;
    assert(exporter->write(ComparisonReport{}, opts) == false);
    exporter = &jsonExporter;
    assert(exporter->write(ComparisonReport{}, opts) == false);

    std::cout << "All Review Module tests PASSED 100% successfully!\n";
    return 0;
}