#include "review/trend.hpp"
#include "review/types.hpp"
#include "analysis/types.hpp"
#include "code/types.hpp"
#include "common/job.hpp"
#include "common/result.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <stop_token>
#include <string>
#include <vector>

using namespace xray;
using namespace xray::review;

// Helper function to create dummy AnalysisSnapshot
static std::shared_ptr<analysis::AnalysisSnapshot> makeSnapshot(
    const std::string& snapId,
    const std::string& sourceId,
    const std::string& fingerprint,
    std::size_t skippedElements = 0,
    Completeness completeness = Completeness::complete)
{
    auto snap = std::make_shared<analysis::AnalysisSnapshot>();
    snap->id = snapId;
    snap->analysisFingerprint = fingerprint;

    auto src = std::make_shared<code::SourceSnapshot>();
    src->id = sourceId;

    auto parsed = std::make_shared<code::ParsedSnapshot>();
    parsed->source = src;
    snap->parsed = parsed;

    snap->coverage.completeness = completeness;
    snap->coverage.skippedElements = skippedElements;

    return snap;
}

static void addFunctionWithMetric(
    std::shared_ptr<analysis::AnalysisSnapshot> snap,
    const std::string& fnId,
    const std::string& key,
    const std::string& relPath,
    analysis::MetricId metricId,
    std::optional<std::int64_t> val,
    code::Validity validity = code::Validity::valid)
{
    auto parsedMut = std::const_pointer_cast<code::ParsedSnapshot>(snap->parsed);
    if (!parsedMut) {
        parsedMut = std::make_shared<code::ParsedSnapshot>();
        snap->parsed = parsedMut;
    }

    code::ParsedUnit unit;
    unit.relativePath = relPath;
    auto fn = std::make_shared<code::FunctionEntity>();
    fn->id = fnId;
    fn->identityKey = key;
    fn->relativePath = relPath;
    fn->range = {0, 100, 1, 10};
    unit.functions.push_back(fn);
    parsedMut->units.push_back(unit);

    analysis::MetricResult m;
    m.entityId = fnId;
    m.metricId = metricId;
    m.value = val;
    m.validity = validity;
    snap->metrics.push_back(m);
}

// ─── Tests for D17 extractTrendPoint ────────────────────────

static void test_extractTrendPoint_function_valid()
{
    auto snap = makeSnapshot("snap1", "src1", "fp_test");
    addFunctionWithMetric(snap, "fn1", "calculateTotal()", "calc.cpp",
                          analysis::MetricId::function_lines, 42);

    TrendOptions opts;
    opts.aggregation = TrendAggregation::function;
    opts.entityKey = "calculateTotal()";
    opts.metricId = analysis::MetricId::function_lines;

    TrendPoint pt = extractTrendPoint(*snap, opts);
    assert(pt.sourceId == "src1");
    assert(pt.validFunctions == 1);
    assert(pt.unknownFunctions == 0);
    assert(pt.value.has_value());
    assert(*pt.value == 42.0);
    std::cout << "  [OK] extractTrendPoint: function valid\n";
}

static void test_extractTrendPoint_function_missing()
{
    auto snap = makeSnapshot("snap1", "src1", "fp_test");

    TrendOptions opts;
    opts.aggregation = TrendAggregation::function;
    opts.entityKey = "nonExistent()";

    TrendPoint pt = extractTrendPoint(*snap, opts);
    assert(pt.sourceId == "src1");
    assert(pt.validFunctions == 0);
    assert(pt.unknownFunctions == 1);
    assert(!pt.value.has_value());
    std::cout << "  [OK] extractTrendPoint: function missing\n";
}

static void test_extractTrendPoint_function_metric_unavailable()
{
    auto snap = makeSnapshot("snap1", "src1", "fp_test");
    addFunctionWithMetric(snap, "fn1", "renderUI()", "ui.cpp",
                          analysis::MetricId::max_nesting,
                          std::nullopt, code::Validity::unavailable);

    TrendOptions opts;
    opts.aggregation = TrendAggregation::function;
    opts.entityKey = "renderUI()";
    opts.metricId = analysis::MetricId::max_nesting;

    TrendPoint pt = extractTrendPoint(*snap, opts);
    assert(pt.validFunctions == 0);
    assert(pt.unknownFunctions == 1);
    assert(!pt.value.has_value());
    std::cout << "  [OK] extractTrendPoint: function metric unavailable\n";
}

static void test_extractTrendPoint_file_mean_and_max()
{
    auto snap = makeSnapshot("snap1", "src1", "fp_test");
    addFunctionWithMetric(snap, "fn1", "f1()", "algo.cpp",
                          analysis::MetricId::function_lines, 10);
    addFunctionWithMetric(snap, "fn2", "f2()", "algo.cpp",
                          analysis::MetricId::function_lines, 30);
    addFunctionWithMetric(snap, "fn3", "f3()", "algo.cpp",
                          analysis::MetricId::function_lines,
                          std::nullopt, code::Validity::unavailable);

    TrendOptions optsMean;
    optsMean.aggregation = TrendAggregation::file_mean;
    optsMean.metricId = analysis::MetricId::function_lines;

    TrendPoint ptMean = extractTrendPoint(*snap, optsMean);
    assert(ptMean.validFunctions == 2);
    assert(ptMean.unknownFunctions == 1);
    assert(ptMean.value.has_value());
    assert(*ptMean.value == 20.0); // (10 + 30) / 2

    TrendOptions optsMax;
    optsMax.aggregation = TrendAggregation::file_max;
    optsMax.metricId = analysis::MetricId::function_lines;

    TrendPoint ptMax = extractTrendPoint(*snap, optsMax);
    assert(ptMax.validFunctions == 2);
    assert(ptMax.unknownFunctions == 1);
    assert(ptMax.value.has_value());
    assert(*ptMax.value == 30.0); // max(10, 30)
    std::cout << "  [OK] extractTrendPoint: file_mean and file_max\n";
}

static void test_extractTrendPoint_duplicate_keys_yield_nullopt_ambiguous()
{
    // [P2] Дубльований ключ: дві функції з однаковим identityKey і метриками 10 та 90
    auto snap = makeSnapshot("snap_dup", "src_dup", "fp_test");
    addFunctionWithMetric(snap, "fn_dup1", "dupMethod()", "dup.cpp",
                          analysis::MetricId::function_lines, 10);
    addFunctionWithMetric(snap, "fn_dup2", "dupMethod()", "dup.cpp",
                          analysis::MetricId::function_lines, 90);

    TrendOptions opts;
    opts.aggregation = TrendAggregation::function;
    opts.entityKey = "dupMethod()";
    opts.metricId = analysis::MetricId::function_lines;

    TrendPoint pt = extractTrendPoint(*snap, opts);
    // Неоднозначність має давати nullopt із поясненням
    assert(!pt.value.has_value());
    assert(pt.validFunctions == 0);
    assert(pt.unknownFunctions == 2);
    assert(pt.reason.find("Ambiguous") != std::string::npos);

    // Перевірка при зворотній послідовності функцій
    auto snapRev = makeSnapshot("snap_dup_rev", "src_dup_rev", "fp_test");
    addFunctionWithMetric(snapRev, "fn_dup2", "dupMethod()", "dup.cpp",
                          analysis::MetricId::function_lines, 90);
    addFunctionWithMetric(snapRev, "fn_dup1", "dupMethod()", "dup.cpp",
                          analysis::MetricId::function_lines, 10);

    TrendPoint ptRev = extractTrendPoint(*snapRev, opts);
    assert(!ptRev.value.has_value());
    assert(ptRev.validFunctions == 0);
    assert(ptRev.unknownFunctions == 2);
    assert(ptRev.reason.find("Ambiguous") != std::string::npos);

    std::cout << "  [OK] extractTrendPoint: duplicate keys yield nullopt with Ambiguous reason regardless of order\n";
}

static void test_extractTrendPoint_syntactic_only_function_and_aggregates()
{
    // [P2] syntactic_only: дозволяє числові значення з попередженням
    // 1. Функціональний тренд для syntactic_only
    auto snapFunc = makeSnapshot("snap_syn", "src_syn", "fp_test");
    addFunctionWithMetric(snapFunc, "fn_syn", "macroFunc()", "macro.cpp",
                          analysis::MetricId::function_lines, 42,
                          code::Validity::syntactic_only);

    TrendOptions optsFunc;
    optsFunc.aggregation = TrendAggregation::function;
    optsFunc.entityKey = "macroFunc()";
    optsFunc.metricId = analysis::MetricId::function_lines;

    TrendPoint ptFunc = extractTrendPoint(*snapFunc, optsFunc);
    assert(ptFunc.validFunctions == 1);
    assert(ptFunc.unknownFunctions == 0);
    assert(ptFunc.value.has_value());
    assert(*ptFunc.value == 42.0);
    assert(!ptFunc.reason.empty());

    // 2. Агрегація: валідний 10 і syntactic_only 42
    auto snapAgg = makeSnapshot("snap_agg", "src_agg", "fp_test");
    addFunctionWithMetric(snapAgg, "fn_val", "regularFunc()", "mixed.cpp",
                          analysis::MetricId::function_lines, 10,
                          code::Validity::valid);
    addFunctionWithMetric(snapAgg, "fn_syn", "macroFunc()", "mixed.cpp",
                          analysis::MetricId::function_lines, 42,
                          code::Validity::syntactic_only);

    TrendOptions optsMean;
    optsMean.aggregation = TrendAggregation::file_mean;
    optsMean.metricId = analysis::MetricId::function_lines;

    TrendPoint ptMean = extractTrendPoint(*snapAgg, optsMean);
    assert(ptMean.validFunctions == 2);
    assert(ptMean.unknownFunctions == 0);
    assert(ptMean.value.has_value());
    // Середнє: (10 + 42) / 2 = 26
    assert(*ptMean.value == 26.0);
    assert(ptMean.reason.find("syntactic_only") != std::string::npos);

    TrendOptions optsMax;
    optsMax.aggregation = TrendAggregation::file_max;
    optsMax.metricId = analysis::MetricId::function_lines;

    TrendPoint ptMax = extractTrendPoint(*snapAgg, optsMax);
    assert(ptMax.validFunctions == 2);
    assert(ptMax.unknownFunctions == 0);
    assert(ptMax.value.has_value());
    // Максимум: max(10, 42) = 42
    assert(*ptMax.value == 42.0);
    assert(ptMax.reason.find("syntactic_only") != std::string::npos);

    std::cout << "  [OK] extractTrendPoint: syntactic_only preserved for function, file_mean (26) and file_max (42)\n";
}

// ─── Tests for D18 splitTrendSegments ───────────────────────

static void test_splitTrendSegments()
{
    std::vector<TrendPoint> points;
    auto makePt = [](std::optional<double> v) {
        TrendPoint p;
        p.value = v;
        return p;
    };

    points.push_back(makePt(10.0));
    points.push_back(makePt(20.0));
    points.push_back(makePt(std::nullopt)); // gap
    points.push_back(makePt(30.0));
    points.push_back(makePt(40.0));
    points.push_back(makePt(50.0));
    points.push_back(makePt(std::nullopt)); // gap
    points.push_back(makePt(std::nullopt)); // consecutive gap
    points.push_back(makePt(60.0));

    auto segments = splitTrendSegments(points);
    assert(segments.size() == 3);
    assert(segments[0].size() == 2);
    assert(segments[0][0].value == 10.0);
    assert(segments[0][1].value == 20.0);

    assert(segments[1].size() == 3);
    assert(segments[1][0].value == 30.0);
    assert(segments[1][2].value == 50.0);

    assert(segments[2].size() == 1);
    assert(segments[2][0].value == 60.0);

    std::cout << "  [OK] splitTrendSegments: correctly splits across gaps\n";
}

// ─── Tests for D16 buildTrend ───────────────────────────────

static void test_buildTrend_validation_and_errors()
{
    JobContext ctx;
    TrendOptions opts;

    // 1. Empty snapshots list
    OrderedAnalyses emptyInputs;
    auto resEmpty = buildTrend(emptyInputs, opts, ctx);
    assert(std::holds_alternative<Error>(resEmpty));
    assert(std::get<Error>(resEmpty).code == ErrorCode::invalid_input);

    // 2. Null snapshot
    OrderedAnalyses nullInputs;
    nullInputs.snapshots.push_back(nullptr);
    auto resNull = buildTrend(nullInputs, opts, ctx);
    assert(std::holds_alternative<Error>(resNull));
    assert(std::get<Error>(resNull).code == ErrorCode::invalid_input);

    // 3. Empty fingerprint
    auto snapEmptyFp = makeSnapshot("s1", "src1", "");
    OrderedAnalyses emptyFpInputs;
    emptyFpInputs.snapshots.push_back(snapEmptyFp);
    auto resEmptyFp = buildTrend(emptyFpInputs, opts, ctx);
    assert(std::holds_alternative<Error>(resEmptyFp));
    assert(std::get<Error>(resEmptyFp).code == ErrorCode::incompatible_analysis);

    // 4. Mismatched fingerprints
    auto snap1 = makeSnapshot("s1", "src1", "fp_A");
    auto snap2 = makeSnapshot("s2", "src2", "fp_B");
    OrderedAnalyses mismatchInputs;
    mismatchInputs.snapshots = {snap1, snap2};
    auto resMismatch = buildTrend(mismatchInputs, opts, ctx);
    assert(std::holds_alternative<Error>(resMismatch));
    assert(std::get<Error>(resMismatch).code == ErrorCode::incompatible_analysis);

    std::cout << "  [OK] buildTrend: handles invalid inputs and fingerprint mismatches\n";
}

static void test_buildTrend_cancellation()
{
    std::stop_source stopSource;
    stopSource.request_stop();
    JobContext ctx{1, stopSource.get_token()};

    auto snap = makeSnapshot("s1", "src1", "fp_ok");
    OrderedAnalyses inputs;
    inputs.snapshots.push_back(snap);

    TrendOptions opts;
    auto res = buildTrend(inputs, opts, ctx);
    assert(std::holds_alternative<Cancelled>(res));
    std::cout << "  [OK] buildTrend: supports cancellation\n";
}

static void test_buildTrend_limits_and_coverage()
{
    JobContext ctx;
    OrderedAnalyses inputs;

    for (int i = 1; i <= 5; ++i) {
        auto snap = makeSnapshot(
            "s" + std::to_string(i),
            "src" + std::to_string(i),
            "fp_shared",
            /*skippedElements=*/i,
            /*completeness=*/(i == 4) ? Completeness::partial : Completeness::complete
        );
        addFunctionWithMetric(snap, "fn", "target()", "main.cpp",
                              analysis::MetricId::function_lines, i * 10);
        inputs.snapshots.push_back(snap);
    }

    TrendOptions opts;
    opts.entityKey = "target()";
    opts.metricId = analysis::MetricId::function_lines;
    opts.limit = 3; // Should keep the last 3 snapshots: s3, s4, s5

    auto res = buildTrend(inputs, opts, ctx);
    assert(std::holds_alternative<TrendReport>(res));
    const auto& report = std::get<TrendReport>(res);

    assert(report.points.size() == 3);
    assert(report.points[0].sourceId == "src3");
    assert(report.points[0].value == 30.0);
    assert(report.points[1].sourceId == "src4");
    assert(report.points[1].value == 40.0);
    assert(report.points[2].sourceId == "src5");
    assert(report.points[2].value == 50.0);

    // Coverage: s3 skipped 3, s4 skipped 4, s5 skipped 5 -> sum = 12
    assert(report.coverage.skippedElements == 12);
    // Because s4 is partial, report coverage must be partial
    assert(report.coverage.completeness == Completeness::partial);

    std::cout << "  [OK] buildTrend: limits and coverage aggregation verified\n";
}

// ─── main ───────────────────────────────────────────────────

int main()
{
    std::cout << "=== trend tests ===\n";
    test_extractTrendPoint_function_valid();
    test_extractTrendPoint_function_missing();
    test_extractTrendPoint_function_metric_unavailable();
    test_extractTrendPoint_file_mean_and_max();
    test_extractTrendPoint_duplicate_keys_yield_nullopt_ambiguous();
    test_extractTrendPoint_syntactic_only_function_and_aggregates();

    test_splitTrendSegments();

    test_buildTrend_validation_and_errors();
    test_buildTrend_cancellation();
    test_buildTrend_limits_and_coverage();

    std::cout << "\nAll PR3 trend_tests PASSED 100%!\n";
    return 0;
}
