#include "review/matching.hpp"
#include "review/rules.hpp"
#include "review/exporter.hpp"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <cstdint>

int main() {
    using namespace xray::review;

    // === 1. matchEntities: matched / added / removed ===
    struct DummyEntity {
        std::string name;
        std::int64_t metric;
    };

    std::vector<DummyEntity> base = {
        {"func_match", 100},
        {"func_remove", 50}
    };
    std::vector<DummyEntity> target = {
        {"func_match", 120},
        {"func_add", 30}
    };

    auto results = matchEntities(
        base, target,
        [](const DummyEntity& e) { return e.name; },
        [](const DummyEntity& e) { return e.metric; }
    );

    assert(results.size() == 3);

    // Знайти результат за ключем
    auto findByKey = [&](const std::string& key) -> const EntityReview<std::string, std::int64_t>* {
        for (const auto& r : results) {
            if (r.key == key) return &r;
        }
        return nullptr;
    };

    auto* revMatch = findByKey("func_match");
    assert(revMatch != nullptr);
    assert(revMatch->kind == MatchKind::matched);
    assert(revMatch->metricDiff == 20); // 120 - 100

    auto* revRemove = findByKey("func_remove");
    assert(revRemove != nullptr);
    assert(revRemove->kind == MatchKind::removed);

    auto* revAdd = findByKey("func_add");
    assert(revAdd != nullptr);
    assert(revAdd->kind == MatchKind::added);
    assert(revAdd->metricDiff == 30);

    std::cout << "  matchEntities: OK\n";

    // === 2. joinByKey: дублікати ключів -> ambiguous ===
    std::vector<DummyEntity> baseDup = {
        {"dup_key", 10},
        {"dup_key", 20},  // дублікат
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

    // "dup_key" має бути ambiguous (2 зліва, 1 справа)
    std::size_t ambiguousCount = 0;
    std::size_t matchedCount = 0;
    for (const auto& r : dupResults) {
        if (r.key == "dup_key") {
            assert(r.kind == MatchKind::ambiguous);
            ++ambiguousCount;
        }
        if (r.key == "unique") {
            assert(r.kind == MatchKind::matched);
            assert(r.metricDiff == 10); // 40 - 30
            ++matchedCount;
        }
    }
    assert(ambiguousCount == 3); // 2 зліва + 1 справа
    assert(matchedCount == 1);

    std::cout << "  duplicate keys -> ambiguous: OK\n";

    // === 3. Правила (ComparisonRule) через базовий клас ===
    MetricGrowthRule growthRule(10);
    NewFindingRule newRule;
    ResolvedFindingRule resolvedRule;

    EntityReview<std::string, std::int64_t> revAdded{"f1", MatchKind::added, 5};
    EntityReview<std::string, std::int64_t> revRemoved{"f2", MatchKind::removed, -10};
    EntityReview<std::string, std::int64_t> revSmallGrowth{"f3", MatchKind::matched, 8};
    EntityReview<std::string, std::int64_t> revBigGrowth{"f4", MatchKind::matched, 15};

    // MetricGrowthRule: metricDiff > поріг
    assert(growthRule.evaluate(revSmallGrowth) == false); // 8 <= 10
    assert(growthRule.evaluate(revBigGrowth) == true);    // 15 > 10

    // NewFindingRule: kind == added
    assert(newRule.evaluate(revAdded) == true);
    assert(newRule.evaluate(revRemoved) == false);

    // ResolvedFindingRule: kind == removed
    assert(resolvedRule.evaluate(revRemoved) == true);
    assert(resolvedRule.evaluate(revAdded) == false);

    // Виклик через базовий клас (поліморфізм)
    ComparisonRule* rule = &growthRule;
    assert(rule->name() == "MetricGrowthRule");
    assert(rule->evaluate(revBigGrowth) == true);

    std::cout << "  ComparisonRule hierarchy: OK\n";

    // === 4. Експортери (ReportExporter) через базовий клас ===
    HtmlExporter htmlExporter;
    JsonExporter jsonExporter;

    ExportOptions opts;
    opts.destination = "report.html";

    // TODO: stub — реальний експорт у файл буде в наступному PR
    ReportExporter* exporter = &htmlExporter;
    assert(exporter->write(ComparisonReport{}, opts) == true);

    exporter = &jsonExporter;
    assert(exporter->write(ComparisonReport{}, opts) == true);

    // Порожній шлях -> false
    ExportOptions emptyOpts;
    assert(htmlExporter.write(ComparisonReport{}, emptyOpts) == false);

    std::cout << "  ReportExporter hierarchy: OK\n";

    std::cout << "All review tests PASSED!\n";
    return 0;
}