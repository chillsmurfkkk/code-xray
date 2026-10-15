#include "analysis/engine.hpp"
#include "analysis/aggregation.hpp"
#include "analysis/profiles.hpp"
#include "review/api.hpp"
#include "review/trend.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <stop_token>
using namespace xray;
using namespace xray::analysis;
namespace {
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class T> T take(Result<T> result) {
    if (const auto* error = std::get_if<Error>(&result)) throw std::runtime_error(error->message);
    check(std::holds_alternative<T>(result), "Unexpected cancellation");
    return std::move(std::get<T>(result));
}
std::shared_ptr<code::ParsedSnapshot> input() {
    auto parsed = std::make_shared<code::ParsedSnapshot>();
    auto source = std::make_shared<code::SourceSnapshot>(); source->id = "before"; parsed->source = source;
    code::ParsedUnit unit; unit.relativePath = "test.cpp";
    for (int i = 0; i < 4; ++i) {
        auto fn = std::make_shared<code::FunctionEntity>(); fn->id = std::to_string(i); fn->name = "f" + fn->id;
        fn->identityKey = "key" + fn->id; fn->relativePath = unit.relativePath;
        fn->range = {0, 100, 1, 70}; fn->bodyRange = fn->range;
        fn->validity = i == 0 ? code::Validity::valid : i == 1 ? code::Validity::syntactic_only :
            i == 2 ? code::Validity::unavailable : code::Validity::not_applicable;
        fn->reason = "test precision";
        if (i == 3) fn->bodyRange.reset();
        code::ControlNode node; node.kind = code::ControlKind::if_statement;
        for (int j = 0; j < 11; ++j) fn->controlTree.children.push_back(node);
        unit.functions.push_back(fn);
    }
    parsed->units.push_back(unit); return parsed;
}
void facadeAndCancellation() {
    auto parsed = input();
    std::vector<Progress> events;
    JobContext job{12, {}, [&](const auto& event) { events.push_back(event); }};
    auto snapshot = take(analyze(parsed, {}, job));
    check(snapshot.metrics.size() == 12 && snapshot.findings.size() == 4, "All metrics and two rules must run");
    check(snapshot.parsed == parsed && snapshot.coverage.completeness == Completeness::partial, "Ownership and coverage must survive");
    check(validateAnalysisResult(snapshot).empty(), "Generated result must validate");
    check(events.size() == 5 && events.front().completed == 0 && events.back().completed == 4 &&
          events.back().jobId == 12 && events.back().total == 4, "Progress includes job ID and all functions");
    auto nestedInput = input();
    auto nested = std::make_shared<code::FunctionEntity>(*nestedInput->units[0].functions[0]);
    code::ControlNode chain; chain.kind = code::ControlKind::if_statement;
    for (int i = 1; i < 4; ++i) { code::ControlNode outer; outer.kind = code::ControlKind::for_loop; outer.children.push_back(chain); chain = outer; }
    nested->controlTree.children.push_back(chain);
    nestedInput->units[0].functions = {nested};
    auto allRules = take(analyze(nestedInput, {}, {}));
    check(allRules.findings.size() == 3 && validateAnalysisResult(allRules).empty(), "All three rules dispatch through the engine");
    check(allRules.coverage.completeness == Completeness::complete, "Valid input remains complete");
    auto profile = snapshot.profile; profile.enabledRules.fill(false);
    const auto disabled = take(analyze(parsed, profile, {}));
    check(disabled.findings.empty() && disabled.metrics.size() == 12, "Disabling rules must keep all metrics");
    check(validateAnalysisResult(disabled).empty(), "Disabled result must validate");
    const auto summary = summarizeCoverage(*parsed);
    check(summary.valid == 1 && summary.syntacticOnly == 1 && summary.unavailable == 1 && summary.notApplicable == 1, "Coverage categories count functions once");
    for (std::size_t stopAt : {0u, 1u, 4u}) {
        std::stop_source stop;
        JobContext cancelled{1, stop.get_token(), [&](const auto& event) { if (event.completed == stopAt) stop.request_stop(); }};
        auto result = analyze(parsed, {}, cancelled);
        check(std::holds_alternative<Cancelled>(result), "Cancellation at any progress boundary discards snapshot");
    }
    std::stop_source stop; stop.request_stop();
    check(std::holds_alternative<Cancelled>(analyze(nullptr, {}, {1, stop.get_token(), {}})), "Pre-cancelled request never starts");
    check(std::holds_alternative<Error>(analyze(nullptr, {}, {})), "Null input fails");
    parsed->units[0].functions.push_back(parsed->units[0].functions.front());
    check(std::holds_alternative<Error>(analyze(parsed, {}, {})), "Duplicate function ID fails");
    parsed = input(); parsed->units[0].functions.push_back(nullptr);
    check(std::holds_alternative<Error>(analyze(parsed, {}, {})), "Null indexed function fails");
    parsed = input(); parsed->units.clear();
    check(take(analyze(parsed, {}, {})).metrics.empty(), "Empty analysis is a success");
    parsed = input(); parsed->coverage = {Completeness::partial, 7, {{ErrorCode::read_error, "skip", "lost.cpp", {}, {}}}};
    const auto partial = take(analyze(parsed, {}, {}));
    check(partial.coverage.skippedElements == 7 && partial.coverage.diagnostics.size() == 1, "Inherit coverage exactly once");
    // A GUI callback can edit the active profile, but the running job uses its copy.
    AnalysisProfile mutableProfile;
    JobContext edits{1, {}, [&](const auto&) { mutableProfile.longFunctionLimit = 100; }};
    const auto copied = take(analyze(input(), mutableProfile, edits));
    check(copied.profile.longFunctionLimit == 60 && copied.findings.size() == 4, "Running profile is immutable");
}
void fingerprintsAndIntegration() {
    auto a = input(); auto b = input();
    auto src = std::make_shared<code::SourceSnapshot>(*b->source);
    src->id = "after"; src->commitOid = "other-commit";
    src->files.push_back({"different.cpp", std::make_shared<const std::string>("other bytes"), "content"}); b->source = src;
    src->selection.extensions = {".h", ".cpp", ".cpp"};
    auto aSrc = std::make_shared<code::SourceSnapshot>(*a->source); aSrc->selection.extensions = {".cpp", ".h"}; a->source = aSrc;
    AnalysisProfile profile; profile.id = "copy"; profile.name = "Another name";
    const auto fp = take(makeAnalysisFingerprint(*a, {}));
    check(fp == take(makeAnalysisFingerprint(*b, profile)), "Fingerprint ignores source data and metadata; canonicalizes selection");
    for (int choice = 0; choice < 7; ++choice) {
        auto changed = profile; auto parsed = *b;
        if (choice == 0) changed.nestingLimit++;
        if (choice == 1) changed.longFunctionLimit++;
        if (choice == 2) changed.branchLimit++;
        if (choice == 3) changed.enabledRules[0] = false;
        if (choice == 4) parsed.options.grammarVersion += "new";
        if (choice == 5) parsed.options.structureVersion += "new";
        if (choice == 6) { auto source = std::make_shared<code::SourceSnapshot>(*parsed.source); source->selection.relativePaths = {"x.cpp"}; parsed.source = source; }
        check(fp != take(makeAnalysisFingerprint(parsed, changed)), "Behavior changes must alter fingerprint");
    }
    auto before = std::make_shared<AnalysisSnapshot>(take(analyze(a, {}, {})));
    auto after = std::make_shared<AnalysisSnapshot>(take(analyze(b, profile, {})));
    auto report = review::compare(before, after, {true}, {});
    check(std::holds_alternative<review::ComparisonReport>(report), "Real analysis snapshots must feed comparison");
    review::TrendOptions options; options.entityKey = "key0";
    const auto trend = review::buildTrend({"tip", {before, after}}, options, {});
    check(std::holds_alternative<review::TrendReport>(trend), "Real snapshots feed trend");
    check(take(analyze(a, {}, {})).id == before->id, "Repeated inputs have stable IDs");
    auto invalidProfile = profile; invalidProfile.schemaVersion = 9;
    check(std::holds_alternative<Error>(analyze(a, invalidProfile, {})), "Invalid profile fails");
    auto registry = take(buildMetricRegistry()); registry.clear();
    check(std::holds_alternative<Error>(resolveEnabledRules({}, registry)), "Rule dependencies are checked");
}
void aggregatesAndViews() {
    const auto snapshot = take(analyze(input(), {}, {}));
    const auto mean = aggregateMetrics(snapshot.metrics, MetricId::function_lines, MeanReducer{});
    const auto maximum = aggregateMetrics(snapshot.metrics, MetricId::function_lines, MaxReducer{});
    check(mean.value == 70 && mean.included == 2 && mean.unknown == 1 && mean.notApplicable == 1 && mean.syntacticOnly == 1, "Mean denominator excludes unknown and declarations");
    check(maximum.value == 70 && maximum.maxima == std::vector<std::string>{"0", "1"}, "Maximum retains all tied entities");
    check(!aggregateMetrics(std::vector<MetricResult>{}, MetricId::function_lines, MeanReducer{}).value, "Empty mean is unknown");
    check(!aggregateMetrics(std::vector<MetricResult>{}, MetricId::function_lines, MaxReducer{}).value, "Empty maximum is unknown");
    const std::vector<MetricResult> large{{"a", MetricId::function_lines, std::numeric_limits<std::int64_t>::max(), code::Validity::valid, {}}, {"b", MetricId::function_lines, std::numeric_limits<std::int64_t>::max(), code::Validity::valid, {}}};
    check(aggregateMetrics(large, MetricId::function_lines, MeanReducer{}).value.value() > 9e18, "Mean does not overflow signed integer");
    FindingFilter filter; filter.rule = RuleId::long_function; filter.pathContains = "test"; filter.textContains = "f1"; filter.validity = code::Validity::syntactic_only;
    check(filterFindings(snapshot, filter).size() == 1, "Combined filters select matching finding");
    filter.pathContains = "absent"; check(filterFindings(snapshot, filter).empty(), "Missing file filter is empty");
    const auto groups = groupFindingsByFile(snapshot);
    check(groups.size() == 1 && groups.at("test.cpp").size() == 4, "Findings grouped by source file");
    check(rankFunctions(snapshot, MetricId::function_lines) == std::vector<std::string>{"0", "1", "2", "3"}, "Ranking is stable with unknown last");
    auto varied = snapshot; varied.metrics[0].value = 80;
    check(rankFunctions(varied, MetricId::function_lines).front() == "0", "Descending ranking uses numeric values");
    check(rankFunctions(varied, MetricId::function_lines, false).front() == "1", "Ascending ranking keeps unknown last");

}
void validationRejectsCorruption() {
    const auto good = take(analyze(input(), {}, {}));
    for (int choice = 0; choice < 9; ++choice) {
        auto bad = good;
        if (choice == 0) bad.metrics.push_back(bad.metrics.front());
        if (choice == 1) bad.metrics.pop_back();
        if (choice == 2) bad.metrics[0].entityId = "missing";
        if (choice == 3) bad.findings[0].threshold++;
        if (choice == 4) bad.findings.clear();
        if (choice == 5) bad.analysisFingerprint.clear();
        if (choice == 6) bad.metrics[0].value.reset();
        if (choice == 7) bad.coverage.completeness = Completeness::complete;
        if (choice == 8) bad.findings.push_back(bad.findings.front());
        check(!validateAnalysisResult(bad).empty(), "Corrupt result must be rejected");
    }
}
} // namespace
int main() {
    try { facadeAndCancellation(); fingerprintsAndIntegration(); aggregatesAndViews(); validationRejectsCorruption();
        std::cout << "Analysis engine tests passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
