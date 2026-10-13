#include "analysis/metrics.hpp"
#include "analysis/rules.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

void check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Helper to construct a FunctionEntity for tests
std::shared_ptr<xray::code::FunctionEntity> makeFunction(
    const std::string& id,
    const std::string& name,
    std::uint32_t startLine,
    std::uint32_t endLine,
    bool hasBody = true,
    xray::code::Validity validity = xray::code::Validity::valid) {

    auto fn = std::make_shared<xray::code::FunctionEntity>();
    fn->id = id;
    fn->name = name;
    fn->relativePath = "src/test.cpp";
    fn->range = {0, 100, startLine, endLine};
    if (hasBody) {
        fn->bodyRange = xray::code::SourceRange{10, 100, startLine, endLine};
    } else {
        fn->bodyRange = std::nullopt;
    }
    fn->validity = validity;
    return fn;
}

void testSingleLineFunction() {
    xray::analysis::LengthMetric metric;
    auto fn = makeFunction("fn1", "singleLine", 1, 1);

    auto result = metric.calculate(*fn);
    check(result.metricId == xray::analysis::MetricId::function_lines, "metricId must be function_lines");
    check(result.value.has_value(), "value must be present");
    check(*result.value == 1, "single line function must have 1 line");
    check(result.validity == xray::code::Validity::valid, "validity must be valid");

    xray::analysis::LongFunctionRule rule;
    xray::analysis::AnalysisProfile profile;
    auto finding = rule.evaluate(*fn, result, profile);
    check(!finding.has_value(), "1-line function must not violate long function rule");
}

void testMultiLineFunction() {
    xray::analysis::LengthMetric metric;
    auto fn = makeFunction("fn2", "multiLine", 10, 24);

    auto result = metric.calculate(*fn);
    check(result.value.has_value() && *result.value == 15, "10 to 24 inclusive must be 15 lines");
    check(result.validity == xray::code::Validity::valid, "validity must be valid");
}

void testDeclarationWithoutBody() {
    xray::analysis::LengthMetric metric;
    auto fn = makeFunction("fn3", "declOnly", 5, 5, /*hasBody=*/false);

    auto result = metric.calculate(*fn);
    check(!result.value.has_value(), "declaration without body must not have value");
    check(result.validity == xray::code::Validity::not_applicable, "validity must be not_applicable");
    check(!result.reason.empty(), "reason must explain missing body");

    xray::analysis::LongFunctionRule rule;
    xray::analysis::AnalysisProfile profile;
    auto finding = rule.evaluate(*fn, result, profile);
    check(!finding.has_value(), "not_applicable metric must not trigger rule");
}

void testUnavailableFunction() {
    xray::analysis::LengthMetric metric;
    auto fn = makeFunction("fn4", "broken", 1, 10, /*hasBody=*/true, xray::code::Validity::unavailable);
    fn->reason = "Syntax error in body";

    auto result = metric.calculate(*fn);
    check(!result.value.has_value(), "unavailable function must not have value");
    check(result.validity == xray::code::Validity::unavailable, "validity must be unavailable");

    xray::analysis::LongFunctionRule rule;
    xray::analysis::AnalysisProfile profile;
    auto finding = rule.evaluate(*fn, result, profile);
    check(!finding.has_value(), "unavailable function must not trigger rule");
}

void testLongFunctionThresholds() {
    xray::analysis::LengthMetric metric;
    xray::analysis::LongFunctionRule rule;
    xray::analysis::AnalysisProfile profile;
    profile.longFunctionLimit = 60;

    // Test 1: Exactly at threshold (60 lines) -> strictly > 60 means NO violation
    auto fn60 = makeFunction("fn60", "exactThreshold", 1, 60);
    auto res60 = metric.calculate(*fn60);
    check(*res60.value == 60, "must be 60 lines");
    auto finding60 = rule.evaluate(*fn60, res60, profile);
    check(!finding60.has_value(), "value equal to threshold must NOT trigger violation");

    // Test 2: Threshold + 1 (61 lines) -> violation!
    auto fn61 = makeFunction("fn61", "overThreshold", 1, 61);
    auto res61 = metric.calculate(*fn61);
    check(*res61.value == 61, "must be 61 lines");
    auto finding61 = rule.evaluate(*fn61, res61, profile);
    check(finding61.has_value(), "value > threshold must trigger violation");
    check(finding61->observedValue == 61, "finding must report observed value 61");
    check(finding61->threshold == 60, "finding must report threshold 60");
    check(finding61->ruleId == xray::analysis::RuleId::long_function, "ruleId must match");
    check(finding61->entityId == "fn61", "entityId must match");
    check(!finding61->explanation.empty(), "explanation must not be empty");

    // Test 3: Rule disabled in profile -> no violation even for 61 lines
    profile.enabledRules[static_cast<std::size_t>(xray::analysis::RuleId::long_function)] = false;
    auto findingDisabled = rule.evaluate(*fn61, res61, profile);
    check(!findingDisabled.has_value(), "disabled rule must not trigger finding");
}

void testPolymorphicDispatch() {
    // Academic OOP Requirement: calls through base class references
    std::unique_ptr<xray::analysis::Metric> metric = std::make_unique<xray::analysis::LengthMetric>();
    std::unique_ptr<xray::analysis::Rule> rule = std::make_unique<xray::analysis::LongFunctionRule>();

    check(metric->id() == xray::analysis::MetricId::function_lines, "polymorphic id() check");
    check(metric->name() == "function_lines", "polymorphic name() check");
    check(rule->id() == xray::analysis::RuleId::long_function, "polymorphic rule id() check");

    auto fn = makeFunction("polyFn", "testPoly", 1, 70);
    xray::analysis::MetricResult result = metric->calculate(*fn);
    check(*result.value == 70, "polymorphic calculate must return 70 lines");

    xray::analysis::AnalysisProfile profile;
    auto finding = rule->evaluate(*fn, result, profile);
    check(finding.has_value() && finding->observedValue == 70, "polymorphic evaluate must find violation");
}

} // namespace

int main() {
    try {
        testSingleLineFunction();
        testMultiLineFunction();
        testDeclarationWithoutBody();
        testUnavailableFunction();
        testLongFunctionThresholds();
        testPolymorphicDispatch();
        std::cout << "All analysis unit tests passed successfully!\n";
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
