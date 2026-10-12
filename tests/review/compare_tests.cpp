// tests/review/compare_tests.cpp
// Тести для PR2: compare, summarize, group, filter, rank, validate.
// Запускається через CTest як compare_tests.

#include "review/api.hpp"
#include "review/matching.hpp"
#include "review/rules.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace xray;
using namespace xray::review;

// ─── helpers ────────────────────────────────────────────────

static std::shared_ptr<analysis::AnalysisSnapshot> makeSnap(
    const std::string& id,
    const std::string& fingerprint,
    std::vector<analysis::MetricResult> metrics = {},
    std::vector<analysis::Finding> findings     = {},
    Completeness completeness                   = Completeness::complete)
{
    auto snap = std::make_shared<analysis::AnalysisSnapshot>();
    snap->id                   = id;
    snap->analysisFingerprint  = fingerprint;
    snap->metricSchemaVersion  = "metrics-v1";
    snap->metrics              = std::move(metrics);
    snap->findings             = std::move(findings);
    snap->coverage.completeness = completeness;
    return snap;
}

static analysis::MetricResult metric(
    const std::string& entityId, analysis::MetricId mid, std::int64_t val)
{
    analysis::MetricResult m;
    m.entityId = entityId;
    m.metricId = mid;
    m.value    = val;
    m.validity = code::Validity::valid;
    return m;
}

static analysis::Finding finding(
    const std::string& entityId, analysis::RuleId rid)
{
    analysis::Finding f;
    f.id            = entityId + ":" + std::to_string(static_cast<int>(rid));
    f.entityId      = entityId;
    f.ruleId        = rid;
    f.observedValue = 100;
    f.threshold     = 60;
    return f;
}

// ─── tests ──────────────────────────────────────────────────

static void test_validateCompatibility_mismatched_fingerprint()
{
    auto base   = makeSnap("b1", "fp-A");
    auto target = makeSnap("t1", "fp-B");   // різний fingerprint

    JobContext ctx;
    auto result = compare(base, target, {}, ctx);
    assert(std::holds_alternative<Error>(result));
    assert(std::get<Error>(result).code == ErrorCode::incompatible_analysis);
    std::cout << "  [OK] incompatible fingerprint -> Error\n";
}

static void test_validateCompatibility_empty_fingerprint()
{
    auto base   = makeSnap("b1", "");        // порожній fingerprint
    auto target = makeSnap("t1", "fp-A");

    JobContext ctx;
    auto result = compare(base, target, {}, ctx);
    assert(std::holds_alternative<Error>(result));
    assert(std::get<Error>(result).code == ErrorCode::incompatible_analysis);
    std::cout << "  [OK] empty fingerprint -> Error\n";
}

static void test_compare_empty_snapshots()
{
    auto base   = makeSnap("b1", "fp-X");
    auto target = makeSnap("t1", "fp-X");

    JobContext ctx;
    auto result = compare(base, target, {}, ctx);
    assert(std::holds_alternative<ComparisonReport>(result));
    const auto& rpt = std::get<ComparisonReport>(result);
    assert(rpt.items.empty());
    assert(rpt.baseId   == "b1");
    assert(rpt.targetId == "t1");
    std::cout << "  [OK] empty snapshots -> empty report\n";
}

static void test_summarize()
{
    std::vector<ReviewItem> items;
    auto makeItem = [](const std::string& id, ReviewKind k) {
        ReviewItem ri;
        ri.id   = id;
        ri.kind = k;
        return ri;
    };
    items.push_back(makeItem("a", ReviewKind::added));
    items.push_back(makeItem("b", ReviewKind::removed));
    items.push_back(makeItem("c", ReviewKind::changed));
    items.push_back(makeItem("d", ReviewKind::changed));
    items.push_back(makeItem("e", ReviewKind::unchanged));
    items.push_back(makeItem("f", ReviewKind::unmatched));
    // Дублікат ID не повинен рахуватися двічі
    items.push_back(makeItem("a", ReviewKind::added));

    std::string s = summarizeComparison(items);
    // Очікуємо added=1 (не 2) — дублікат не рахується
    assert(s.find("added=1")     != std::string::npos);
    assert(s.find("removed=1")   != std::string::npos);
    assert(s.find("changed=2")   != std::string::npos);
    assert(s.find("unchanged=1") != std::string::npos);
    assert(s.find("unmatched=1") != std::string::npos);
    std::cout << "  [OK] summarize: " << s << "\n";
}

static void test_group_sorts_by_path_then_kind()
{
    auto makeItem = [](const std::string& id, ReviewKind k, const std::string& path) {
        ReviewItem ri;
        ri.id   = id;
        ri.kind = k;
        ri.entityKey = id;
        SourceRef sr;
        sr.relativePath = path;
        ri.sourceRefs.push_back(sr);
        return ri;
    };

    std::vector<ReviewItem> items;
    items.push_back(makeItem("z", ReviewKind::unchanged, "b.cpp"));
    items.push_back(makeItem("a", ReviewKind::added,     "a.cpp"));
    items.push_back(makeItem("c", ReviewKind::changed,   "b.cpp"));
    items.push_back(makeItem("r", ReviewKind::removed,   "a.cpp"));

    auto grouped = groupReviewItems(items);
    assert(grouped.size() == 4);
    // a.cpp перед b.cpp
    assert(grouped[0].sourceRefs[0].relativePath == "a.cpp");
    assert(grouped[1].sourceRefs[0].relativePath == "a.cpp");
    // у a.cpp: removed(2) після added(1)? Ні: kind order: changed=0, added=1, removed=2
    // a.cpp має: added(order=1), removed(order=2) -> added перший
    assert(grouped[0].kind == ReviewKind::added);
    assert(grouped[1].kind == ReviewKind::removed);
    // b.cpp: changed(0) перед unchanged(4)
    assert(grouped[2].kind == ReviewKind::changed);
    assert(grouped[3].kind == ReviewKind::unchanged);
    std::cout << "  [OK] group: correct path+kind order\n";
}

static void test_filter_by_text()
{
    auto makeItem = [](const std::string& id, const std::string& key) {
        ReviewItem ri; ri.id = id; ri.entityKey = key;
        ri.kind = ReviewKind::changed;
        ri.confidence = Confidence::exact;
        return ri;
    };
    std::vector<ReviewItem> items;
    items.push_back(makeItem("1", "foo::bar"));
    items.push_back(makeItem("2", "baz::qux"));

    auto filtered = filterReviewItems(items, "foo", "", std::nullopt, true);
    assert(filtered.size() == 1);
    assert(filtered[0].entityKey == "foo::bar");
    std::cout << "  [OK] filter by text\n";
}

static void test_filter_by_kind()
{
    std::vector<ReviewItem> items;
    auto add = [&](const std::string& id, ReviewKind k) {
        ReviewItem ri; ri.id = id; ri.kind = k; ri.confidence = Confidence::exact;
        items.push_back(ri);
    };
    add("1", ReviewKind::added);
    add("2", ReviewKind::removed);
    add("3", ReviewKind::changed);

    auto filtered = filterReviewItems(items, "", "", ReviewKind::added, true);
    assert(filtered.size() == 1);
    assert(filtered[0].kind == ReviewKind::added);
    std::cout << "  [OK] filter by kind\n";
}

static void test_filter_show_unknown()
{
    std::vector<ReviewItem> items;
    ReviewItem ri;
    ri.id         = "u";
    ri.kind       = ReviewKind::unmatched;
    ri.confidence = Confidence::incomplete;
    items.push_back(ri);

    auto withUnknown    = filterReviewItems(items, "", "", std::nullopt, true);
    auto withoutUnknown = filterReviewItems(items, "", "", std::nullopt, false);
    assert(withUnknown.size()    == 1);
    assert(withoutUnknown.size() == 0);
    std::cout << "  [OK] filter showUnknown flag\n";
}

static void test_rank_by_metric_delta()
{
    auto makeItem = [](const std::string& id, std::optional<std::int64_t> delta) {
        ReviewItem ri;
        ri.id = id; ri.entityKey = id;
        ri.kind = ReviewKind::changed;
        MetricDelta md;
        md.metricId = analysis::MetricId::function_lines;
        md.delta = delta;
        ri.metrics.push_back(md);
        return ri;
    };

    std::vector<ReviewItem> items;
    items.push_back(makeItem("small",  5LL));
    items.push_back(makeItem("big",   50LL));
    items.push_back(makeItem("none",  std::nullopt));
    items.push_back(makeItem("neg",  -30LL));

    auto ranked = rankReviewItems(items, analysis::MetricId::function_lines);
    // |50| > |-30| > |5| > nullopt
    assert(ranked[0].entityKey == "big");
    assert(ranked[1].entityKey == "neg");
    assert(ranked[2].entityKey == "small");
    assert(ranked[3].entityKey == "none");
    std::cout << "  [OK] rank: big(50) > neg(-30) > small(5) > none(null)\n";
}

static void test_validate_duplicate_id()
{
    ComparisonReport rpt;
    ReviewItem ri1; ri1.id = "dup"; ri1.entityKey = "f1"; ri1.kind = ReviewKind::added;
    ri1.confidence = Confidence::exact;
    ReviewItem ri2 = ri1; // той самий id
    ri2.entityKey = "f2";
    rpt.items = {ri1, ri2};

    auto errors = validateComparisonReport(rpt);
    bool foundDup = false;
    for (const auto& e : errors) if (e.find("Duplicate") != std::string::npos) foundDup = true;
    assert(foundDup);
    std::cout << "  [OK] validate: duplicate ID detected\n";
}

static void test_validate_removed_with_delta()
{
    ComparisonReport rpt;
    ReviewItem ri;
    ri.id = "r1"; ri.entityKey = "func"; ri.kind = ReviewKind::removed;
    ri.confidence = Confidence::exact;
    MetricDelta md; md.metricId = analysis::MetricId::function_lines; md.delta = 10; // неправильно!
    ri.metrics.push_back(md);
    rpt.items = {ri};

    auto errors = validateComparisonReport(rpt);
    bool foundDelta = false;
    for (const auto& e : errors) if (e.find("delta") != std::string::npos) foundDelta = true;
    assert(foundDelta);
    std::cout << "  [OK] validate: removed with delta detected\n";
}

static void test_validate_ok()
{
    ComparisonReport rpt;
    ReviewItem ri;
    ri.id = "ok1"; ri.entityKey = "func::foo"; ri.kind = ReviewKind::changed;
    ri.confidence = Confidence::exact;
    MetricDelta md; md.metricId = analysis::MetricId::function_lines;
    md.before = 10; md.after = 20; md.delta = 10;
    ri.metrics.push_back(md);
    rpt.items = {ri};

    auto errors = validateComparisonReport(rpt);
    assert(errors.empty());
    std::cout << "  [OK] validate: valid report has no errors\n";
}

// ─── trend tests (D16, D17, D18) ───────────────────────────

#include "review/trend.hpp"
#include "review/exporter.hpp"

static void test_trend()
{
    auto snap1 = makeSnap("commit1", "fp-trend");
    auto snap2 = makeSnap("commit2", "fp-trend");
    auto snap3 = makeSnap("commit3", "fp-trend");

    // Створюємо фіктивну функцію в snap1 та snap3
    auto pu1 = std::make_shared<code::ParsedSnapshot>();
    code::ParsedUnit u1;
    auto fn1 = std::make_shared<code::FunctionEntity>();
    fn1->id = "fn1";
    fn1->identityKey = "app::process";
    fn1->relativePath = "app.cpp";
    u1.functions.push_back(fn1);
    pu1->units.push_back(u1);
    snap1->parsed = pu1;
    snap1->metrics.push_back(metric("fn1", analysis::MetricId::function_lines, 25));

    // snap2 не містить функції (видалена або не аналізувалася)
    snap2->parsed = std::make_shared<code::ParsedSnapshot>();

    // snap3 містить функцію з іншим значенням
    auto pu3 = std::make_shared<code::ParsedSnapshot>();
    code::ParsedUnit u3;
    auto fn3 = std::make_shared<code::FunctionEntity>();
    fn3->id = "fn3";
    fn3->identityKey = "app::process";
    fn3->relativePath = "app.cpp";
    u3.functions.push_back(fn3);
    pu3->units.push_back(u3);
    snap3->parsed = pu3;
    snap3->metrics.push_back(metric("fn3", analysis::MetricId::function_lines, 40));

    TrendOptions opts;
    opts.metricId = analysis::MetricId::function_lines;
    opts.aggregation = TrendAggregation::function;
    opts.entityKey = "app::process";

    // D17: extractTrendPoint
    auto pt1 = extractTrendPoint(*snap1, opts);
    assert(pt1.value.has_value() && *pt1.value == 25.0);
    assert(pt1.validFunctions == 1);

    auto pt2 = extractTrendPoint(*snap2, opts);
    assert(!pt2.value.has_value()); // gap
    assert(pt2.unknownFunctions == 1);

    auto pt3 = extractTrendPoint(*snap3, opts);
    assert(pt3.value.has_value() && *pt3.value == 40.0);

    // D18: splitTrendSegments (пропуск не повинен малювати суцільну лінію)
    std::vector<TrendPoint> pts = {pt1, pt2, pt3};
    auto segs = splitTrendSegments(pts);
    assert(segs.size() == 2);
    assert(segs[0].size() == 1 && segs[0][0].value == 25.0);
    assert(segs[1].size() == 1 && segs[1][0].value == 40.0);
    std::cout << "  [OK] trend segments: gap correctly split into 2 segments\n";

    // D16: buildTrend
    OrderedAnalyses ordered;
    ordered.chainTipOid = "commit3";
    ordered.snapshots = {snap1, snap2, snap3};

    JobContext ctx;
    auto trendRes = buildTrend(ordered, opts, ctx);
    assert(std::holds_alternative<TrendReport>(trendRes));
    const auto& tr = std::get<TrendReport>(trendRes);
    assert(tr.points.size() == 3);
    assert(tr.analysisFingerprint == "fp-trend");
    std::cout << "  [OK] buildTrend: collected 3 points with correct fingerprint\n";
}

// ─── exporter tests (D20, D21, D22, D23, D24) ──────────────

static void test_exporter()
{
    // D21: escapeHtmlText
    std::string raw = "<script>alert('xss' & \"test\");</script>";
    std::string escaped = escapeHtmlText(raw);
    assert(escaped.find("<script>") == std::string::npos);
    assert(escaped.find("&lt;script&gt;") != std::string::npos);
    assert(escaped.find("&amp;") != std::string::npos);
    assert(escaped.find("&#39;") != std::string::npos);
    assert(escaped.find("&quot;") != std::string::npos);

    // Уникнення подвійного екранування
    std::string alreadyEscaped = "hello &amp; world";
    assert(escapeHtmlText(alreadyEscaped) == "hello &amp; world");
    std::cout << "  [OK] escapeHtmlText: safe and avoids double escaping\n";

    // D20, D22, D23: створюємо тестовий звіт
    ComparisonReport report;
    report.baseId = "base-v1";
    report.targetId = "target-v2";
    report.analysisFingerprint = "fp-test";
    report.metricSchemaVersion = "metrics-v1";

    ReviewItem item;
    item.id = "item1";
    item.kind = ReviewKind::changed;
    item.entityKey = "renderOrder";
    item.explanation = "Changed length";
    item.confidence = Confidence::exact;
    MetricDelta md;
    md.metricId = analysis::MetricId::function_lines;
    md.before = 20; md.after = 50; md.delta = 30;
    item.metrics.push_back(md);
    report.items.push_back(item);

    // D20: buildReportModel
    auto model = buildReportModel(report);
    assert(model.totalItems == 1);
    assert(model.rows.size() == 1);
    assert(model.rows[0].entityKey == "renderOrder");

    // D24: writeReportSafely
    auto testDir = std::filesystem::temp_directory_path() / "xray_test";
    std::filesystem::create_directories(testDir);
    auto htmlPath = testDir / "report.html";
    auto jsonPath = testDir / "report.json";

    // D22: HtmlExporter::write
    HtmlExporter htmlExp;
    ExportOptions htmlOpts;
    htmlOpts.destination = htmlPath;
    htmlOpts.format = ExportFormat::html;
    htmlOpts.overwriteConfirmed = true;
    bool htmlOk = htmlExp.write(report, htmlOpts);
    assert(htmlOk);
    assert(std::filesystem::exists(htmlPath));
    assert(std::filesystem::file_size(htmlPath) > 0);
    std::cout << "  [OK] HtmlExporter: written valid HTML\n";

    // D23: JsonExporter::write
    JsonExporter jsonExp;
    ExportOptions jsonOpts;
    jsonOpts.destination = jsonPath;
    jsonOpts.format = ExportFormat::json;
    jsonOpts.overwriteConfirmed = true;
    bool jsonOk = jsonExp.write(report, jsonOpts);
    assert(jsonOk);
    assert(std::filesystem::exists(jsonPath));
    assert(std::filesystem::file_size(jsonPath) > 0);
    std::cout << "  [OK] JsonExporter: written valid JSON\n";

    // Dynamic polymorphism: exportReport фасад
    JobContext ctx;
    auto receipt = exportReport(report, htmlOpts, ctx);
    assert(std::holds_alternative<ExportReceipt>(receipt));
    assert(std::get<ExportReceipt>(receipt).path == htmlPath);
    std::cout << "  [OK] exportReport facade: polymorphic write successful\n";

    // Перевірка захисту від перезапису (overwriteConfirmed = false)
    ExportOptions noOverwrite = htmlOpts;
    noOverwrite.overwriteConfirmed = false;
    assert(!htmlExp.write(report, noOverwrite));
    std::cout << "  [OK] writeReportSafely: refuses overwrite when overwriteConfirmed is false\n";

    // Очищення
    std::filesystem::remove(htmlPath);
    std::filesystem::remove(jsonPath);
    std::filesystem::remove(testDir);
}

// ─── main ───────────────────────────────────────────────────

int main()
{
    std::cout << "=== compare tests ===\n";
    test_validateCompatibility_mismatched_fingerprint();
    test_validateCompatibility_empty_fingerprint();
    test_compare_empty_snapshots();

    std::cout << "\n=== summarize tests ===\n";
    test_summarize();

    std::cout << "\n=== group tests ===\n";
    test_group_sorts_by_path_then_kind();

    std::cout << "\n=== filter tests ===\n";
    test_filter_by_text();
    test_filter_by_kind();
    test_filter_show_unknown();

    std::cout << "\n=== rank tests ===\n";
    test_rank_by_metric_delta();

    std::cout << "\n=== validate tests ===\n";
    test_validate_duplicate_id();
    test_validate_removed_with_delta();
    test_validate_ok();

    std::cout << "\n=== trend tests ===\n";
    test_trend();

    std::cout << "\n=== exporter tests ===\n";
    test_exporter();

    std::cout << "\nAll compare_tests PASSED 100%!\n";
    return 0;
}
