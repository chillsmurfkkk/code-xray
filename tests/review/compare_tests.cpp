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
#include <unordered_set>
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

    auto src = std::make_shared<code::SourceSnapshot>();
    src->id = id + "-source";
    auto parsed = std::make_shared<code::ParsedSnapshot>();
    parsed->source = src;
    snap->parsed = parsed;

    return snap;
}

static analysis::MetricResult metricUnavailable(
    const std::string& entityId, analysis::MetricId mid, const std::string& reason = "unavailable")
{
    analysis::MetricResult m;
    m.entityId = entityId;
    m.metricId = mid;
    m.value    = std::nullopt;
    m.validity = code::Validity::unavailable;
    m.reason   = reason;
    return m;
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
    ri.sourceRefs.push_back({"src_b", "foo.cpp", "ent_b", {0, 10, 1, 2}});
    ri.sourceRefs.push_back({"src_t", "foo.cpp", "ent_t", {0, 20, 1, 3}});
    rpt.items = {ri};

    auto errors = validateComparisonReport(rpt);
    assert(errors.empty());
    std::cout << "  [OK] validate: valid report has no errors\n";
}

// ─── regression tests for PR review probes ──────────────────

static void test_duplicate_keys_handled_without_arbitrary_delta()
{
    // 2 функції у base ("f_dup"), 1 у target ("f_dup")
    auto base = makeSnap("base-snap", "fp-match");
    auto target = makeSnap("target-snap", "fp-match");

    auto makeFn = [](const std::string& id, const std::string& key) {
        auto fn = std::make_shared<code::FunctionEntity>();
        fn->id = id;
        fn->identityKey = key;
        fn->relativePath = "main.cpp";
        fn->range = {0, 100, 1, 10};
        return fn;
    };

    auto bParsed = std::make_shared<code::ParsedSnapshot>();
    bParsed->source = base->parsed->source;
    code::ParsedUnit bUnit;
    bUnit.functions.push_back(makeFn("b_fn1", "f_dup"));
    bUnit.functions.push_back(makeFn("b_fn2", "f_dup"));
    bParsed->units.push_back(bUnit);
    base->parsed = bParsed;
    base->metrics.push_back(metric("b_fn1", analysis::MetricId::function_lines, 10));
    base->metrics.push_back(metric("b_fn2", analysis::MetricId::function_lines, 20));

    auto tParsed = std::make_shared<code::ParsedSnapshot>();
    tParsed->source = target->parsed->source;
    code::ParsedUnit tUnit;
    tUnit.functions.push_back(makeFn("t_fn1", "f_dup"));
    tParsed->units.push_back(tUnit);
    target->parsed = tParsed;
    target->metrics.push_back(metric("t_fn1", analysis::MetricId::function_lines, 12));

    JobContext ctx;
    auto res = compare(base, target, {}, ctx);
    assert(std::holds_alternative<ComparisonReport>(res));
    const auto& rpt = std::get<ComparisonReport>(res);

    // Має бути 3 елементи (2 з base, 1 з target)
    assert(rpt.items.size() == 3);

    // Кожен елемент має бути unmatched / ambiguous
    std::unordered_set<std::string> ids;
    for (const auto& item : rpt.items) {
        assert(item.kind == ReviewKind::unmatched);
        assert(item.confidence == Confidence::ambiguous);
        // P1: Унікальні ID
        assert(ids.insert(item.id).second);
        // P1: Жодної дельти між довільними кандидатами!
        for (const auto& m : item.metrics) {
            assert(!m.delta.has_value());
        }
    }
    std::cout << "  [OK] probe P1: duplicate keys yield 3 unique unmatched items without delta\n";
}

static void test_source_ref_sourceId_and_entityId_correct()
{
    auto base = makeSnap("analysis-base", "fp-sr");
    auto target = makeSnap("analysis-target", "fp-sr");

    auto bSource = std::make_shared<code::SourceSnapshot>();
    bSource->id = "fixture-before";
    auto bParsed = std::make_shared<code::ParsedSnapshot>();
    bParsed->source = bSource;
    code::ParsedUnit bUnit;
    auto bFn = std::make_shared<code::FunctionEntity>();
    bFn->id = "entity_b_42";
    bFn->identityKey = "processOrder";
    bFn->relativePath = "order.cpp";
    bFn->range = {10, 50, 2, 8};
    bUnit.functions.push_back(bFn);
    bParsed->units.push_back(bUnit);
    base->parsed = bParsed;

    auto tSource = std::make_shared<code::SourceSnapshot>();
    tSource->id = "fixture-after";
    auto tParsed = std::make_shared<code::ParsedSnapshot>();
    tParsed->source = tSource;
    code::ParsedUnit tUnit;
    auto tFn = std::make_shared<code::FunctionEntity>();
    tFn->id = "entity_t_99";
    tFn->identityKey = "processOrder";
    tFn->relativePath = "order.cpp";
    tFn->range = {10, 80, 2, 12};
    tUnit.functions.push_back(tFn);
    tParsed->units.push_back(tUnit);
    target->parsed = tParsed;

    base->metrics.push_back(metric("entity_b_42", analysis::MetricId::function_lines, 20));
    target->metrics.push_back(metric("entity_t_99", analysis::MetricId::function_lines, 35));

    JobContext ctx;
    auto res = compare(base, target, {}, ctx);
    assert(std::holds_alternative<ComparisonReport>(res));
    const auto& rpt = std::get<ComparisonReport>(res);
    assert(rpt.items.size() == 1);
    const auto& item = rpt.items.front();

    // P1: sourceId має бути "fixture-before" / "fixture-after" (не AnalysisSnapshot::id)
    assert(item.sourceRefs.size() == 2);
    assert(item.sourceRefs[0].sourceId == "fixture-before");
    assert(item.sourceRefs[1].sourceId == "fixture-after");

    // P1: entityId не порожній, а відповідає FunctionEntity::id
    assert(item.sourceRefs[0].entityId == "entity_b_42");
    assert(item.sourceRefs[1].entityId == "entity_t_99");
    std::cout << "  [OK] probe P1: SourceRef sourceId and entityId correctly populated\n";
}

static void test_unknown_findings_visible_when_includeUnchanged_false()
{
    // Функція присутня в обох, але порушення в target стало unknown
    auto base = makeSnap("b", "fp-unk");
    auto target = makeSnap("t", "fp-unk", {}, {}, Completeness::partial);

    auto makeUnit = [](const std::string& fnId, std::shared_ptr<const code::SourceSnapshot> src) {
        auto p = std::make_shared<code::ParsedSnapshot>();
        p->source = src;
        code::ParsedUnit u;
        auto fn = std::make_shared<code::FunctionEntity>();
        fn->id = fnId;
        fn->identityKey = "calcMetrics";
        fn->relativePath = "calc.cpp";
        u.functions.push_back(fn);
        p->units.push_back(u);
        return p;
    };

    base->parsed = makeUnit("fn_b", base->parsed->source);
    target->parsed = makeUnit("fn_t", target->parsed->source);

    // У base було порушення
    base->findings.push_back(finding("fn_b", analysis::RuleId::long_function));
    // У target порушень немає, але target Completeness::partial -> finding стає unknown

    ComparisonOptions opts;
    opts.includeUnchanged = false; // типовий звіт

    JobContext ctx;
    auto res = compare(base, target, opts, ctx);
    assert(std::holds_alternative<ComparisonReport>(res));
    const auto& rpt = std::get<ComparisonReport>(res);

    // P1: Рядок НЕ зникає! Він повинен бути changed, бо результат невідомий
    assert(rpt.items.size() == 1);
    assert(rpt.items.front().kind == ReviewKind::changed);
    assert(rpt.items.front().confidence == Confidence::incomplete);
    assert(rpt.items.front().explanation.find("unknown") != std::string::npos);
    std::cout << "  [OK] probe P1: unknown finding remains visible when includeUnchanged=false\n";
}

static void test_cancellation_during_building_report()
{
    auto base = makeSnap("b", "fp-canc");
    auto target = makeSnap("t", "fp-canc");

    std::stop_source stopSource;
    JobContext ctx;
    ctx.stopToken = stopSource.get_token();

    // Скасовуємо перед / під час compare
    stopSource.request_stop();

    auto res = compare(base, target, {}, ctx);
    // P2: повертає Cancelled
    assert(std::holds_alternative<Cancelled>(res));
    std::cout << "  [OK] probe P2: cancellation returns Cancelled\n";
}

static void test_coverage_skipped_elements_inherited()
{
    auto base = makeSnap("b", "fp-skip");
    auto target = makeSnap("t", "fp-skip");
    base->coverage.skippedElements = 2;
    target->coverage.skippedElements = 3;

    JobContext ctx;
    auto res = compare(base, target, {}, ctx);
    assert(std::holds_alternative<ComparisonReport>(res));
    const auto& rpt = std::get<ComparisonReport>(res);

    // P2: лічильник пропусків успадковується
    assert(rpt.coverage.skippedElements == 5);
    std::cout << "  [OK] probe P2: skippedElements aggregated (2+3=5)\n";
}

static void test_canonical_file_selection()
{
    auto base = makeSnap("b", "fp-canon");
    auto target = makeSnap("t", "fp-canon");

    auto bSrc = std::make_shared<code::SourceSnapshot>();
    bSrc->id = "b-src";
    bSrc->selection.extensions = {".cpp", ".h"};
    bSrc->selection.excludedDirectories = {"build", "vendor"};
    auto bParsed = std::make_shared<code::ParsedSnapshot>();
    bParsed->source = bSrc;
    base->parsed = bParsed;

    auto tSrc = std::make_shared<code::SourceSnapshot>();
    tSrc->id = "t-src";
    tSrc->selection.extensions = {".h", ".cpp"}; // інший порядок!
    tSrc->selection.excludedDirectories = {"vendor", "build"}; // інший порядок!
    auto tParsed = std::make_shared<code::ParsedSnapshot>();
    tParsed->source = tSrc;
    target->parsed = tParsed;

    JobContext ctx;
    auto res = compare(base, target, {}, ctx);
    // P2: канонічні фільтри визнаються сумісними!
    assert(std::holds_alternative<ComparisonReport>(res));
    std::cout << "  [OK] probe P2: canonical file selection succeeds with reordered lists\n";
}

static void test_probe_unavailable_metrics_not_treated_as_unchanged()
{
    // P1: Функція, де всі метрики unavailable з обох боків, не повинна зникати при includeUnchanged=false!
    auto base = makeSnap("b", "fp-unavail");
    auto target = makeSnap("t", "fp-unavail");

    auto makeFn = [](const std::string& id) {
        auto fn = std::make_shared<code::FunctionEntity>();
        fn->id = id;
        fn->identityKey = "heavyFunc";
        fn->relativePath = "calc.cpp";
        fn->range = {0, 100, 1, 10};
        return fn;
    };

    auto bp = std::make_shared<code::ParsedSnapshot>();
    bp->source = base->parsed->source;
    code::ParsedUnit bu; bu.functions.push_back(makeFn("b_fn"));
    bp->units.push_back(bu);
    base->parsed = bp;

    auto tp = std::make_shared<code::ParsedSnapshot>();
    tp->source = target->parsed->source;
    code::ParsedUnit tu; tu.functions.push_back(makeFn("t_fn"));
    tp->units.push_back(tu);
    target->parsed = tp;

    // Усі 3 метрики недоступні на обох боках
    for (auto mid : {analysis::MetricId::function_lines, analysis::MetricId::max_nesting, analysis::MetricId::branch_count}) {
        base->metrics.push_back(metricUnavailable("b_fn", mid, "parse timeout"));
        target->metrics.push_back(metricUnavailable("t_fn", mid, "parse timeout"));
    }

    ComparisonOptions opts;
    opts.includeUnchanged = false; // типові налаштування

    JobContext ctx;
    auto res = compare(base, target, opts, ctx);
    assert(std::holds_alternative<ComparisonReport>(res));
    const auto& rpt = std::get<ComparisonReport>(res);

    // Функція НЕ повинна зникнути! Вона має бути changed з Confidence::incomplete
    assert(rpt.items.size() == 1);
    assert(rpt.items.front().kind == ReviewKind::changed);
    assert(rpt.items.front().confidence == Confidence::incomplete);
    assert(!rpt.items.front().metrics.empty());
    assert(rpt.items.front().explanation.find("unavailable") != std::string::npos);
    std::cout << "  [OK] probe P1: unavailable metrics kept as changed/incomplete when includeUnchanged=false\n";
}

static void test_probe_syntactic_only_caveat()
{
    // P2: Перевірка попередження syntactic_only у поясненні
    auto base = makeSnap("b", "fp-syn");
    auto target = makeSnap("t", "fp-syn");

    auto makeFn = [](const std::string& id, code::Validity val, const std::string& rsn) {
        auto fn = std::make_shared<code::FunctionEntity>();
        fn->id = id;
        fn->identityKey = "conditionalFunc";
        fn->relativePath = "compat.cpp";
        fn->validity = val;
        fn->reason = rsn;
        fn->range = {0, 50, 1, 5};
        return fn;
    };

    auto bp = std::make_shared<code::ParsedSnapshot>();
    bp->source = base->parsed->source;
    code::ParsedUnit bu; bu.functions.push_back(makeFn("b_syn", code::Validity::valid, ""));
    bp->units.push_back(bu);
    base->parsed = bp;

    auto tp = std::make_shared<code::ParsedSnapshot>();
    tp->source = target->parsed->source;
    code::ParsedUnit tu; tu.functions.push_back(makeFn("t_syn", code::Validity::syntactic_only, "contains #ifdef blocks"));
    tp->units.push_back(tu);
    target->parsed = tp;

    JobContext ctx;
    auto res = compare(base, target, {}, ctx);
    assert(std::holds_alternative<ComparisonReport>(res));
    const auto& rpt = std::get<ComparisonReport>(res);
    assert(rpt.items.size() == 1);
    const auto& item = rpt.items.front();

    // Має бути попередження syntactic_only в поясненні
    assert(item.explanation.find("Warning (syntactic_only)") != std::string::npos);
    assert(item.confidence == Confidence::incomplete);
    std::cout << "  [OK] probe P2: syntactic_only caveat present in explanation\n";
}

static void test_probe_missing_snapshots_return_invalid_input()
{
    // P2: Відсутні ParsedSnapshot або SourceSnapshot повертають invalid_input
    auto base = makeSnap("b", "fp-valid");
    auto target = makeSnap("t", "fp-valid");

    // Прибираємо parsed у base
    base->parsed = nullptr;
    JobContext ctx;
    auto res1 = compare(base, target, {}, ctx);
    assert(std::holds_alternative<Error>(res1));
    assert(std::get<Error>(res1).code == ErrorCode::invalid_input);

    // Відновлюємо parsed без source
    base = makeSnap("b", "fp-valid");
    auto pNoSrc = std::make_shared<code::ParsedSnapshot>();
    pNoSrc->source = nullptr;
    base->parsed = pNoSrc;
    auto res2 = compare(base, target, {}, ctx);
    assert(std::holds_alternative<Error>(res2));
    assert(std::get<Error>(res2).code == ErrorCode::invalid_input);

    std::cout << "  [OK] probe P2: missing Parsed/SourceSnapshot returns invalid_input\n";
}

static void test_probe_filter_show_unknown_completely_removes_ambiguous_and_incomplete()
{
    // P2: showUnknown=false приховує всі неоднозначні та incomplete елементи
    ReviewItem exactItem;
    exactItem.id = "exact1";
    exactItem.entityKey = "func1";
    exactItem.kind = ReviewKind::changed;
    exactItem.confidence = Confidence::exact;
    MetricDelta md1; md1.metricId = analysis::MetricId::function_lines; md1.delta = 5; md1.before = 10; md1.after = 15;
    exactItem.metrics.push_back(md1);

    ReviewItem incompleteFindingItem;
    incompleteFindingItem.id = "inc_find";
    incompleteFindingItem.entityKey = "func2";
    incompleteFindingItem.kind = ReviewKind::changed;
    incompleteFindingItem.confidence = Confidence::incomplete;
    FindingChange fc; fc.state = FindingState::unknown;
    incompleteFindingItem.findings.push_back(fc);

    ReviewItem ambiguousItem;
    ambiguousItem.id = "ambig1";
    ambiguousItem.entityKey = "func3";
    ambiguousItem.kind = ReviewKind::unmatched;
    ambiguousItem.confidence = Confidence::ambiguous;

    std::vector<ReviewItem> items = {exactItem, incompleteFindingItem, ambiguousItem};

    auto shownAll = filterReviewItems(items, "", "", std::nullopt, /*showUnknown=*/true);
    assert(shownAll.size() == 3);

    auto shownOnlyKnown = filterReviewItems(items, "", "", std::nullopt, /*showUnknown=*/false);
    assert(shownOnlyKnown.size() == 1);
    assert(shownOnlyKnown.front().id == "exact1");

    std::cout << "  [OK] probe P2: showUnknown=false completely removes ambiguous and incomplete items\n";
}

static void test_probe_filter_keeps_exact_added_and_removed()
{
    // P2: compare -> filterReviewItems(showUnknown=false) НЕ повинен відкидати коректні додавання й видалення!
    auto base = makeSnap("b", "fp-add-rem");
    auto target = makeSnap("t", "fp-add-rem");

    auto makeFn = [](const std::string& id, const std::string& key) {
        auto fn = std::make_shared<code::FunctionEntity>();
        fn->id = id;
        fn->identityKey = key;
        fn->relativePath = "service.cpp";
        fn->range = {0, 50, 1, 5};
        return fn;
    };

    // base має "oldFunc" (видалена), target має "newFunc" (додана)
    auto bp = std::make_shared<code::ParsedSnapshot>();
    bp->source = base->parsed->source;
    code::ParsedUnit bu;
    bu.functions.push_back(makeFn("fn_old", "oldFunc"));
    bp->units.push_back(bu);
    base->parsed = bp;
    base->metrics.push_back(metric("fn_old", analysis::MetricId::function_lines, 20));

    auto tp = std::make_shared<code::ParsedSnapshot>();
    tp->source = target->parsed->source;
    code::ParsedUnit tu;
    tu.functions.push_back(makeFn("fn_new", "newFunc"));
    tp->units.push_back(tu);
    target->parsed = tp;
    target->metrics.push_back(metric("fn_new", analysis::MetricId::function_lines, 30));

    JobContext ctx;
    auto res = compare(base, target, {}, ctx);
    assert(std::holds_alternative<ComparisonReport>(res));
    const auto& rpt = std::get<ComparisonReport>(res);

    assert(rpt.items.size() == 2);

    // Фільтруємо з showUnknown = false
    auto filtered = filterReviewItems(rpt.items, "", "", std::nullopt, /*showUnknown=*/false);

    // Обидва елементи мають залишитися!
    assert(filtered.size() == 2);
    bool foundAdded = false, foundRemoved = false;
    for (const auto& it : filtered) {
        if (it.kind == ReviewKind::added && it.confidence == Confidence::exact) foundAdded = true;
        if (it.kind == ReviewKind::removed && it.confidence == Confidence::exact) foundRemoved = true;
    }
    assert(foundAdded);
    assert(foundRemoved);

    std::cout << "  [OK] probe P2: compare -> filter(showUnknown=false) keeps exact added and removed items\n";
}

static void test_indexEntities()
{
    // D03: indexEntities
    std::vector<std::string> names = {"alpha", "beta", "alpha", "gamma"};
    auto idx = indexEntities(names, [](const std::string& s) { return s; });
    assert(idx["alpha"].size() == 2);
    assert(idx["beta"].size() == 1);
    assert(idx["gamma"].size() == 1);
    std::cout << "  [OK] D03: indexEntities works correctly\n";
}

static void test_classifyUnmatched()
{
    // D05: classifyUnmatched
    assert(classifyUnmatched(true, Completeness::complete) == MatchKind::added);
    assert(classifyUnmatched(false, Completeness::complete) == MatchKind::removed);
    assert(classifyUnmatched(true, Completeness::partial) == MatchKind::unmatched);
    assert(classifyUnmatched(false, Completeness::partial) == MatchKind::unmatched);
    std::cout << "  [OK] D05: classifyUnmatched works correctly\n";
}

static void test_compareMetricValues()
{
    // D07: compareMetricValues
    assert(compareMetricValues<int>(10, 25).value() == 15);
    assert(compareMetricValues<int>(25, 10).value() == -15);
    assert(!compareMetricValues<int>(std::nullopt, 25).has_value());
    assert(!compareMetricValues<int>(10, std::nullopt).has_value());
    assert(!compareMetricValues<int>(std::nullopt, std::nullopt).has_value());
    std::cout << "  [OK] D07: compareMetricValues works correctly\n";
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

    std::cout << "\n=== review probe regression tests ===\n";
    test_duplicate_keys_handled_without_arbitrary_delta();
    test_source_ref_sourceId_and_entityId_correct();
    test_unknown_findings_visible_when_includeUnchanged_false();
    test_cancellation_during_building_report();
    test_coverage_skipped_elements_inherited();
    test_canonical_file_selection();

    std::cout << "\n=== new review probe tests ===\n";
    test_probe_unavailable_metrics_not_treated_as_unchanged();
    test_probe_syntactic_only_caveat();
    test_probe_missing_snapshots_return_invalid_input();
    test_probe_filter_show_unknown_completely_removes_ambiguous_and_incomplete();
    test_probe_filter_keeps_exact_added_and_removed();
    test_indexEntities();
    test_classifyUnmatched();
    test_compareMetricValues();

    std::cout << "\nAll PR2 compare_tests PASSED 100%!\n";
    return 0;
}

