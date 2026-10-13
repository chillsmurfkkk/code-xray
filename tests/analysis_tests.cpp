#include "analysis/metrics.hpp"
#include "analysis/rules.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

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
        fn->controlTree.kind = xray::code::ControlKind::block;
        fn->controlTree.range = *fn->bodyRange;
    } else {
        fn->bodyRange = std::nullopt;
    }
    fn->validity = validity;
    return fn;
}

// Helper to create a ControlNode
xray::code::ControlNode makeNode(xray::code::ControlKind kind) {
    xray::code::ControlNode node;
    node.kind = kind;
    return node;
}

void testT01SingleLineFunction() {
    // T01: Однорядкова функція без умов -> function_lines=1, max_nesting=0, branch_count=0
    auto fn = makeFunction("fn_t01", "simple", 1, 1);

    xray::analysis::LengthMetric lengthMetric;
    xray::analysis::NestingMetric nestingMetric;
    xray::analysis::BranchCountMetric branchMetric;

    auto lenRes = lengthMetric.calculate(*fn);
    auto nestRes = nestingMetric.calculate(*fn);
    auto branchRes = branchMetric.calculate(*fn);

    check(lenRes.value.has_value() && *lenRes.value == 1, "T01: function_lines must be 1");
    check(nestRes.value.has_value() && *nestRes.value == 0, "T01: max_nesting must be 0");
    check(branchRes.value.has_value() && *branchRes.value == 0, "T01: branch_count must be 0");
}

void testT02SequentialAndNestedControl() {
    // T02: Два послідовні if -> max_nesting=1, branch_count=2
    auto fnSeq = makeFunction("fn_seq", "sequentialIfs", 1, 10);
    fnSeq->controlTree.children.push_back(makeNode(xray::code::ControlKind::if_statement));
    fnSeq->controlTree.children.push_back(makeNode(xray::code::ControlKind::if_statement));

    xray::analysis::NestingMetric nestingMetric;
    xray::analysis::BranchCountMetric branchMetric;

    auto nestSeq = nestingMetric.calculate(*fnSeq);
    auto branchSeq = branchMetric.calculate(*fnSeq);

    check(nestSeq.value.has_value() && *nestSeq.value == 1, "T02: sequential ifs nesting must be 1");
    check(branchSeq.value.has_value() && *branchSeq.value == 2, "T02: sequential ifs branches must be 2");

    // T02: if усередині for -> max_nesting=2, branch_count=2
    auto fnNested = makeFunction("fn_nested", "ifInsideFor", 1, 10);
    auto forLoop = makeNode(xray::code::ControlKind::for_loop);
    forLoop.children.push_back(makeNode(xray::code::ControlKind::if_statement));
    fnNested->controlTree.children.push_back(std::move(forLoop));

    auto nestNested = nestingMetric.calculate(*fnNested);
    auto branchNested = branchMetric.calculate(*fnNested);

    check(nestNested.value.has_value() && *nestNested.value == 2, "T02: if inside for nesting must be 2");
    check(branchNested.value.has_value() && *branchNested.value == 2, "T02: if inside for branches must be 2");
}

void testT03ComplexControlConstructs() {
    xray::analysis::NestingMetric nestingMetric;
    xray::analysis::BranchCountMetric branchMetric;

    // 1. else if: той самий рівень, що й if
    auto fnElseIf = makeFunction("fn_elseif", "testElseIf", 1, 10);
    fnElseIf->controlTree.children.push_back(makeNode(xray::code::ControlKind::if_statement));
    fnElseIf->controlTree.children.push_back(makeNode(xray::code::ControlKind::else_if));

    auto nestElseIf = nestingMetric.calculate(*fnElseIf);
    auto branchElseIf = branchMetric.calculate(*fnElseIf);
    check(nestElseIf.value.has_value() && *nestElseIf.value == 1, "T03: else if must stay at nesting 1");
    check(branchElseIf.value.has_value() && *branchElseIf.value == 2, "T03: if + else_if must count 2 branches");

    // 2. switch з 3 case і 1 default
    auto fnSwitch = makeFunction("fn_switch", "testSwitch", 1, 15);
    auto switchNode = makeNode(xray::code::ControlKind::switch_statement);
    switchNode.children.push_back(makeNode(xray::code::ControlKind::case_label));
    switchNode.children.push_back(makeNode(xray::code::ControlKind::case_label));
    switchNode.children.push_back(makeNode(xray::code::ControlKind::case_label));
    switchNode.children.push_back(makeNode(xray::code::ControlKind::default_label));
    fnSwitch->controlTree.children.push_back(std::move(switchNode));

    auto nestSwitch = nestingMetric.calculate(*fnSwitch);
    auto branchSwitch = branchMetric.calculate(*fnSwitch);
    check(nestSwitch.value.has_value() && *nestSwitch.value == 1, "T03: switch must give nesting 1");
    check(branchSwitch.value.has_value() && *branchSwitch.value == 3, "T03: 3 cases (without default) must count 3 branches");

    // 3. try/catch: catch на тому ж рівні, що й try; try не додає гілок, catch додає 1
    auto fnTry = makeFunction("fn_try", "testTryCatch", 1, 10);
    fnTry->controlTree.children.push_back(makeNode(xray::code::ControlKind::try_statement));
    fnTry->controlTree.children.push_back(makeNode(xray::code::ControlKind::catch_clause));

    auto nestTry = nestingMetric.calculate(*fnTry);
    auto branchTry = branchMetric.calculate(*fnTry);
    check(nestTry.value.has_value() && *nestTry.value == 1, "T03: try/catch must have nesting 1");
    check(branchTry.value.has_value() && *branchTry.value == 1, "T03: try/catch must have 1 branch (from catch)");

    // 4. Тернарний оператор conditional_expression: не збільшує вкладеність, але додає 1 гілку
    auto fnTernary = makeFunction("fn_ternary", "testTernary", 1, 5);
    fnTernary->controlTree.children.push_back(makeNode(xray::code::ControlKind::conditional_expression));

    auto nestTernary = nestingMetric.calculate(*fnTernary);
    auto branchTernary = branchMetric.calculate(*fnTernary);
    check(nestTernary.value.has_value() && *nestTernary.value == 0, "T03: ternary operator must not increase nesting");
    check(branchTernary.value.has_value() && *branchTernary.value == 1, "T03: ternary operator must count 1 branch");

    // 5. Вкладені лямбди: їхній вміст пропускається (pruned)
    auto fnLambda = makeFunction("fn_lambda", "testLambda", 1, 10);
    auto lambdaNode = makeNode(xray::code::ControlKind::lambda);
    lambdaNode.children.push_back(makeNode(xray::code::ControlKind::if_statement));
    lambdaNode.children.push_back(makeNode(xray::code::ControlKind::for_loop));
    fnLambda->controlTree.children.push_back(std::move(lambdaNode));

    auto nestLambda = nestingMetric.calculate(*fnLambda);
    auto branchLambda = branchMetric.calculate(*fnLambda);
    check(nestLambda.value.has_value() && *nestLambda.value == 0, "T03: constructs inside lambda must be ignored for nesting");
    check(branchLambda.value.has_value() && *branchLambda.value == 0, "T03: constructs inside lambda must be ignored for branches");
}

void testDeclarationsAndUnavailable() {
    xray::analysis::LengthMetric lengthMetric;
    xray::analysis::NestingMetric nestingMetric;
    xray::analysis::BranchCountMetric branchMetric;

    // Declaration without body
    auto declOnly = makeFunction("decl", "onlyHeader", 1, 1, /*hasBody=*/false);
    check(lengthMetric.calculate(*declOnly).validity == xray::code::Validity::not_applicable, "decl length not_applicable");
    check(nestingMetric.calculate(*declOnly).validity == xray::code::Validity::not_applicable, "decl nesting not_applicable");
    check(branchMetric.calculate(*declOnly).validity == xray::code::Validity::not_applicable, "decl branches not_applicable");
    check(!lengthMetric.calculate(*declOnly).value.has_value(), "decl value must be nullopt");

    // Unavailable function
    auto broken = makeFunction("broken", "syntaxError", 1, 5, /*hasBody=*/true, xray::code::Validity::unavailable);
    check(lengthMetric.calculate(*broken).validity == xray::code::Validity::unavailable, "broken length unavailable");
    check(nestingMetric.calculate(*broken).validity == xray::code::Validity::unavailable, "broken nesting unavailable");
    check(branchMetric.calculate(*broken).validity == xray::code::Validity::unavailable, "broken branches unavailable");
    check(!nestingMetric.calculate(*broken).value.has_value(), "broken value must be nullopt");
}

void testLongFunctionThresholds() {
    xray::analysis::LengthMetric metric;
    xray::analysis::LongFunctionRule rule;
    xray::analysis::AnalysisProfile profile;
    profile.longFunctionLimit = 60;

    auto fn60 = makeFunction("fn60", "exactThreshold", 1, 60);
    auto res60 = metric.calculate(*fn60);
    check(!rule.evaluate(*fn60, res60, profile).has_value(), "equality to limit is not a violation");

    auto fn61 = makeFunction("fn61", "overThreshold", 1, 61);
    auto res61 = metric.calculate(*fn61);
    auto finding = rule.evaluate(*fn61, res61, profile);
    check(finding.has_value() && finding->observedValue == 61, "61 lines must trigger finding");
}

void testElseIfNestedNoBracesRegression() {
    // Regression test: Вкладений if без дужок, де гілка else if містить for -> while.
    // За контрактом розбору A, if (b) та else_if (c) є прямими сусідами всередині if (a).
    // Рівні: if(a) [1] -> { if(b) [2], else_if(c) [2] -> for [3] -> while [4] }
    // Очікувана максимальна вкладеність: 4
    auto fn = makeFunction("fn_nested_elseif", "nestedNoBraces", 1, 15);

    auto ifA = makeNode(xray::code::ControlKind::if_statement);
    auto ifB = makeNode(xray::code::ControlKind::if_statement);
    auto elseIfC = makeNode(xray::code::ControlKind::else_if);
    auto forLoop = makeNode(xray::code::ControlKind::for_loop);
    auto whileLoop = makeNode(xray::code::ControlKind::while_loop);

    forLoop.children.push_back(std::move(whileLoop));
    elseIfC.children.push_back(std::move(forLoop));

    // ifB та elseIfC є сусідами всередині ifA
    ifA.children.push_back(std::move(ifB));
    ifA.children.push_back(std::move(elseIfC));

    fn->controlTree.children.push_back(std::move(ifA));

    xray::analysis::NestingMetric nestingMetric;
    auto result = nestingMetric.calculate(*fn);

    check(result.value.has_value(), "regression: result value must be present");
    check(*result.value == 4, "regression: nested if without braces with else if -> for -> while must have nesting 4");
}

void testDynamicPolymorphismAllMetrics() {
    // Academic OOP Requirement 1: calls through base class Metric& for all three metrics
    std::vector<std::unique_ptr<xray::analysis::Metric>> metrics;
    metrics.push_back(std::make_unique<xray::analysis::LengthMetric>());
    metrics.push_back(std::make_unique<xray::analysis::NestingMetric>());
    metrics.push_back(std::make_unique<xray::analysis::BranchCountMetric>());

    auto fn = makeFunction("polyFn", "testPoly", 1, 20);
    auto whileLoop = makeNode(xray::code::ControlKind::while_loop);
    whileLoop.children.push_back(makeNode(xray::code::ControlKind::if_statement));
    fn->controlTree.children.push_back(std::move(whileLoop));

    std::vector<xray::analysis::MetricResult> results;
    for (const auto& m : metrics) {
        results.push_back(m->calculate(*fn));
    }

    check(results.size() == 3, "must calculate all 3 metrics polymorphically");
    check(results[0].metricId == xray::analysis::MetricId::function_lines && *results[0].value == 20, "lines poly");
    check(results[1].metricId == xray::analysis::MetricId::max_nesting && *results[1].value == 2, "nesting poly");
    check(results[2].metricId == xray::analysis::MetricId::branch_count && *results[2].value == 2, "branches poly");
}

void testDynamicPolymorphismRules() {
    // Academic OOP Requirement 2: calls through base class Rule&
    std::unique_ptr<xray::analysis::Rule> rule = std::make_unique<xray::analysis::LongFunctionRule>();
    check(rule->id() == xray::analysis::RuleId::long_function, "polymorphic rule id() check");
    check(rule->name() == "long_function", "polymorphic rule name() check");

    auto fn = makeFunction("rulePolyFn", "testRulePoly", 1, 70);
    xray::analysis::LengthMetric metric;
    auto metricResult = metric.calculate(*fn);

    xray::analysis::AnalysisProfile profile;
    profile.longFunctionLimit = 60;

    // Call through base class reference Rule&
    const xray::analysis::Rule& baseRuleRef = *rule;
    auto finding = baseRuleRef.evaluate(*fn, metricResult, profile);
    check(finding.has_value(), "polymorphic rule evaluation must detect violation");
    check(finding->observedValue == 70, "observed value must match");
    check(finding->threshold == 60, "threshold must match");
}

} // namespace

int main() {
    try {
        testT01SingleLineFunction();
        testT02SequentialAndNestedControl();
        testT03ComplexControlConstructs();
        testElseIfNestedNoBracesRegression();
        testDeclarationsAndUnavailable();
        testLongFunctionThresholds();
        testDynamicPolymorphismAllMetrics();
        testDynamicPolymorphismRules();
        std::cout << "All analysis unit tests passed successfully!\n";
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
