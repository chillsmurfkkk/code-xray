#include "analysis/engine.hpp"
#include "analysis/aggregation.hpp"
#include <iostream>

int main() {
    using namespace xray;
    using namespace xray::analysis;
    auto source = std::make_shared<code::SourceSnapshot>(); source->id = "demo";
    auto parsed = std::make_shared<code::ParsedSnapshot>(); parsed->source = source;
    auto function = std::make_shared<code::FunctionEntity>();
    function->id = "demo-function"; function->name = "process"; function->relativePath = "demo.cpp";
    function->range = {0, 100, 1, 61}; function->bodyRange = function->range; function->validity = code::Validity::valid;
    code::ParsedUnit unit; unit.relativePath = "demo.cpp"; unit.functions.push_back(function); parsed->units.push_back(unit);
    const auto result = analyze(parsed, {}, {});
    const auto* snapshot = std::get_if<AnalysisSnapshot>(&result);
    if (!snapshot) return 1;
    // analyze uses Metric& and Rule&; these instantiate two template strategies.
    const auto mean = aggregateMetrics(snapshot->metrics, MetricId::function_lines, MeanReducer{});
    const auto maximum = aggregateMetrics(snapshot->metrics, MetricId::function_lines, MaxReducer{});
    std::cout << "Metrics: " << snapshot->metrics.size() << "; findings: " << snapshot->findings.size()
              << "; mean lines: " << *mean.value << "; max lines: " << *maximum.value << '\n';
    for (const auto& finding : snapshot->findings) std::cout << finding.explanation << '\n';
    return validateAnalysisResult(*snapshot).empty() ? 0 : 1;
}
