#include "review/rules.hpp"
#include "review/exporter.hpp"
#include "review/matching.hpp"
#include <iostream>
#include <cassert>

int main() {
    using namespace xray::review;

    // 1. Перевірка усіх 3 правил з rules.hpp
    MetricGrowthRule growthRule(10);
    assert(growthRule.name() == "MetricGrowthRule");
    assert(growthRule.evaluateGrowth(10, 30) == true); // 30 - 10 = 20 > 10

    NewFindingRule newRule;
    assert(newRule.name() == "NewFindingRule");
    assert(newRule.evaluateFinding(FindingState::new_finding) == true);

    ResolvedFindingRule resolvedRule;
    assert(resolvedRule.name() == "ResolvedFindingRule");
    assert(resolvedRule.evaluateFinding(FindingState::resolved) == true);

    // 2. Перевірка обох експортерів з exporter.hpp
    ExportOptions opts;
    opts.destination = "report.html";

    HtmlExporter htmlExporter;
    assert(htmlExporter.write(ComparisonReport{}, opts) == true);

    JsonExporter jsonExporter;
    assert(jsonExporter.write(ComparisonReport{}, opts) == true);

    // 3. Перевірка шаблону зіставлення з matching.hpp
    std::vector<std::string> list1 = {"funcA", "funcB"};
    std::vector<std::string> list2 = {"funcB", "funcC"};
    std::size_t matches = joinByKey(list1, list2, [](const std::string& s) { return s; });
    assert(matches == 1);

    std::cout << "All Review Module tests PASSED 100% successfully!\n";
    return 0;
}