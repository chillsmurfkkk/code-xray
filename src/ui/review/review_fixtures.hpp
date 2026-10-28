#pragma once
// review_fixtures.hpp — демонстраційні дані для UI-сторінок review (D-UI1–D-UI4).
// Позначені як DEMO; замінюються координатором при підключенні до реального сценарію.

#include "review/types.hpp"
#include "review/api.hpp"
#include "analysis/types.hpp"
#include "code/types.hpp"

#include <memory>
#include <string>
#include <vector>

namespace xray::ui::review {

/// Побудувати демонстраційний ComparisonReport із різними видами змін.
inline xray::review::ComparisonReport makeDemoComparisonReport() {
    using namespace xray::review;
    using namespace xray::analysis;
    using namespace xray::code;

    // --- Source bytes (shared) ---
    auto baseBytes = std::make_shared<const std::string>(
        "int add(int a, int b) {\n    return a + b;\n}\n\n"
        "int multiply(int a, int b) {\n    int result = 0;\n"
        "    for (int i = 0; i < b; ++i) result += a;\n    return result;\n}\n\n"
        "void oldFunction() {\n    // removed\n}\n");

    auto targetBytes = std::make_shared<const std::string>(
        "int add(int a, int b) {\n    return a + b;\n}\n\n"
        "int multiply(int a, int b) {\n    int result = a * b;\n    return result;\n}\n\n"
        "void newHelper() {\n    // added\n}\n");

    // --- Source files ---
    SourceFile baseFile{"src/math.cpp", baseBytes, "hash-base-math"};
    SourceFile targetFile{"src/math.cpp", targetBytes, "hash-target-math"};

    // --- SourceSnapshots ---
    auto baseSrc = std::make_shared<SourceSnapshot>();
    baseSrc->id = "src-base-001";
    baseSrc->kind = SourceKind::local;
    baseSrc->files = {baseFile};

    auto targetSrc = std::make_shared<SourceSnapshot>();
    targetSrc->id = "src-target-001";
    targetSrc->kind = SourceKind::local;
    targetSrc->files = {targetFile};

    // --- FunctionEntities (base) ---
    auto fnAddBase = std::make_shared<FunctionEntity>();
    fnAddBase->id = "fn-add-base";
    fnAddBase->name = "add";
    fnAddBase->identityKey = "add(int,int)";
    fnAddBase->relativePath = "src/math.cpp";
    fnAddBase->range = {0, 43, 1, 3};
    fnAddBase->validity = Validity::valid;

    auto fnMulBase = std::make_shared<FunctionEntity>();
    fnMulBase->id = "fn-mul-base";
    fnMulBase->name = "multiply";
    fnMulBase->identityKey = "multiply(int,int)";
    fnMulBase->relativePath = "src/math.cpp";
    fnMulBase->range = {45, 159, 5, 9};
    fnMulBase->validity = Validity::valid;

    auto fnOldBase = std::make_shared<FunctionEntity>();
    fnOldBase->id = "fn-old-base";
    fnOldBase->name = "oldFunction";
    fnOldBase->identityKey = "oldFunction()";
    fnOldBase->relativePath = "src/math.cpp";
    fnOldBase->range = {161, 198, 11, 13};
    fnOldBase->validity = Validity::valid;

    // --- FunctionEntities (target) ---
    auto fnAddTarget = std::make_shared<FunctionEntity>();
    fnAddTarget->id = "fn-add-target";
    fnAddTarget->name = "add";
    fnAddTarget->identityKey = "add(int,int)";
    fnAddTarget->relativePath = "src/math.cpp";
    fnAddTarget->range = {0, 43, 1, 3};
    fnAddTarget->validity = Validity::valid;

    auto fnMulTarget = std::make_shared<FunctionEntity>();
    fnMulTarget->id = "fn-mul-target";
    fnMulTarget->name = "multiply";
    fnMulTarget->identityKey = "multiply(int,int)";
    fnMulTarget->relativePath = "src/math.cpp";
    fnMulTarget->range = {45, 118, 5, 8};
    fnMulTarget->validity = Validity::valid;

    auto fnNewTarget = std::make_shared<FunctionEntity>();
    fnNewTarget->id = "fn-new-target";
    fnNewTarget->name = "newHelper";
    fnNewTarget->identityKey = "newHelper()";
    fnNewTarget->relativePath = "src/math.cpp";
    fnNewTarget->range = {120, 153, 10, 12};
    fnNewTarget->validity = Validity::valid;

    // --- ParsedUnits ---
    ParsedUnit baseUnit;
    baseUnit.relativePath = "src/math.cpp";
    baseUnit.functions = {fnAddBase, fnMulBase, fnOldBase};

    ParsedUnit targetUnit;
    targetUnit.relativePath = "src/math.cpp";
    targetUnit.functions = {fnAddTarget, fnMulTarget, fnNewTarget};

    // --- ParsedSnapshots ---
    auto baseParsed = std::make_shared<ParsedSnapshot>();
    baseParsed->source = baseSrc;
    baseParsed->units = {baseUnit};

    auto targetParsed = std::make_shared<ParsedSnapshot>();
    targetParsed->source = targetSrc;
    targetParsed->units = {targetUnit};

    // --- AnalysisSnapshots ---
    auto baseSnap = std::make_shared<AnalysisSnapshot>();
    baseSnap->id = "analysis-base-001";
    baseSnap->parsed = baseParsed;
    baseSnap->analysisFingerprint = "fp-demo-v1";
    baseSnap->metricSchemaVersion = "metrics-v1";
    baseSnap->coverage.completeness = Completeness::complete;
    baseSnap->metrics = {
        {"fn-add-base", MetricId::function_lines, 3, Validity::valid, ""},
        {"fn-add-base", MetricId::max_nesting, 0, Validity::valid, ""},
        {"fn-mul-base", MetricId::function_lines, 5, Validity::valid, ""},
        {"fn-mul-base", MetricId::max_nesting, 1, Validity::valid, ""},
        {"fn-mul-base", MetricId::branch_count, 1, Validity::valid, ""},
        {"fn-old-base", MetricId::function_lines, 3, Validity::valid, ""},
    };
    baseSnap->findings = {
        {"f1", RuleId::long_function, "fn-mul-base", 5, 4, Validity::valid, "Function exceeds 4 lines"},
    };

    auto targetSnap = std::make_shared<AnalysisSnapshot>();
    targetSnap->id = "analysis-target-001";
    targetSnap->parsed = targetParsed;
    targetSnap->analysisFingerprint = "fp-demo-v1";
    targetSnap->metricSchemaVersion = "metrics-v1";
    targetSnap->coverage.completeness = Completeness::complete;
    targetSnap->metrics = {
        {"fn-add-target", MetricId::function_lines, 3, Validity::valid, ""},
        {"fn-add-target", MetricId::max_nesting, 0, Validity::valid, ""},
        {"fn-mul-target", MetricId::function_lines, 4, Validity::valid, ""},
        {"fn-mul-target", MetricId::max_nesting, 0, Validity::valid, ""},
        {"fn-mul-target", MetricId::branch_count, 0, Validity::valid, ""},
        {"fn-new-target", MetricId::function_lines, 3, Validity::valid, ""},
    };

    // --- Build ComparisonReport ---
    ComparisonReport report;
    report.baseId = "analysis-base-001";
    report.targetId = "analysis-target-001";
    report.analysisFingerprint = "fp-demo-v1";
    report.metricSchemaVersion = "metrics-v1";
    report.base = baseSnap;
    report.target = targetSnap;
    report.coverage.completeness = Completeness::complete;

    // unchanged: add
    {
        ReviewItem item;
        item.id = "demo:unchanged:add";
        item.kind = ReviewKind::unchanged;
        item.entityKey = "add(int,int)";
        item.before = "fn-add-base";
        item.after = "fn-add-target";
        item.explanation = "Function 'add(int,int)' is present in both snapshots with no detected changes.";
        item.confidence = Confidence::exact;
        item.sourceRefs = {
            {"src-base-001", "src/math.cpp", "fn-add-base", {0, 43, 1, 3}},
            {"src-target-001", "src/math.cpp", "fn-add-target", {0, 43, 1, 3}},
        };
        item.metrics = {
            {MetricId::function_lines, 3, 3, 0},
            {MetricId::max_nesting, 0, 0, 0},
        };
        report.items.push_back(std::move(item));
    }

    // changed: multiply (lines 5→4, nesting 1→0, finding resolved)
    {
        ReviewItem item;
        item.id = "demo:changed:mul";
        item.kind = ReviewKind::changed;
        item.entityKey = "multiply(int,int)";
        item.before = "fn-mul-base";
        item.after = "fn-mul-target";
        item.explanation = "Function 'multiply(int,int)' changed between snapshots."
                           " Metric 'lines': -1 (5 → 4). Metric 'nesting depth': -1 (1 → 0)."
                           " 1 resolved finding(s).";
        item.confidence = Confidence::exact;
        item.sourceRefs = {
            {"src-base-001", "src/math.cpp", "fn-mul-base", {45, 159, 5, 9}},
            {"src-target-001", "src/math.cpp", "fn-mul-target", {45, 118, 5, 8}},
        };
        item.metrics = {
            {MetricId::function_lines, 5, 4, -1},
            {MetricId::max_nesting, 1, 0, -1},
            {MetricId::branch_count, 1, 0, -1},
        };
        item.findings = {
            {RuleId::long_function, FindingState::resolved},
        };
        report.items.push_back(std::move(item));
    }

    // removed: oldFunction
    {
        ReviewItem item;
        item.id = "demo:removed:old";
        item.kind = ReviewKind::removed;
        item.entityKey = "oldFunction()";
        item.before = "fn-old-base";
        item.after = std::nullopt;
        item.explanation = "Function 'oldFunction()' was removed in the target snapshot.";
        item.confidence = Confidence::exact;
        item.sourceRefs = {
            {"src-base-001", "src/math.cpp", "fn-old-base", {161, 198, 11, 13}},
        };
        item.metrics = {
            {MetricId::function_lines, 3, std::nullopt, std::nullopt},
        };
        report.items.push_back(std::move(item));
    }

    // added: newHelper
    {
        ReviewItem item;
        item.id = "demo:added:new";
        item.kind = ReviewKind::added;
        item.entityKey = "newHelper()";
        item.before = std::nullopt;
        item.after = "fn-new-target";
        item.explanation = "Function 'newHelper()' was added in the target snapshot.";
        item.confidence = Confidence::exact;
        item.sourceRefs = {
            {"src-target-001", "src/math.cpp", "fn-new-target", {120, 153, 10, 12}},
        };
        item.metrics = {
            {MetricId::function_lines, std::nullopt, 3, std::nullopt},
        };
        report.items.push_back(std::move(item));
    }

    return report;
}

/// Побудувати демонстраційний TrendReport.
inline xray::review::TrendReport makeDemoTrendReport() {
    using namespace xray::review;

    TrendReport trend;
    trend.options.metricId = analysis::MetricId::function_lines;
    trend.options.aggregation = TrendAggregation::function;
    trend.options.entityKey = "multiply(int,int)";
    trend.analysisFingerprint = "fp-demo-v1";

    trend.points = {
        {"commit-aaa", 8.0, 1, 0, ""},
        {"commit-bbb", 7.0, 1, 0, ""},
        {"commit-ccc", std::nullopt, 0, 1, "Function not found in this commit"},
        {"commit-ddd", 5.0, 1, 0, ""},
        {"commit-eee", 4.0, 1, 0, ""},
    };
    trend.coverage.completeness = Completeness::complete;

    return trend;
}

} // namespace xray::ui::review
