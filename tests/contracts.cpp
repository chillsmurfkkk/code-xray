#include "code/api.hpp"
#include "analysis/api.hpp"
#include "history/api.hpp"
#include "review/api.hpp"
#include "fixtures.hpp"

#include <iostream>
#include <stdexcept>
#include <type_traits>

namespace {
void check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void lifetimeAndRanges() {
    const auto before = fixtures::analysis(false);
    const auto after = fixtures::analysis(true);
    const auto& unit = before->parsed->units.front();
    const auto& function = *unit.functions.front();
    const auto& bytes = *before->parsed->source->files.front().bytes;
    check(bytes.substr(function.range.startByte, function.range.endByte - function.range.startByte)
              == "int f() { return 1; }", "range must address retained source bytes");
    check(unit.root->children.front().get() == unit.functions.front().get(), "index must share entity ownership");
    check(unit.root->sourceBytes == before->parsed->source->files.front().bytes, "source must not be reread");
    check(function.identityKey == after->parsed->units.front().functions.front()->identityKey,
          "line changes must preserve matching key");
    check(function.range.endLine == 1 && after->parsed->units.front().functions.front()->range.endLine == 3,
          "last token lines must be inclusive");
    check(before->metrics.front().value == 1 && after->metrics.front().value == 3,
          "fixture length expectation changed");
    const xray::code::CodeEntity& entity = function;
    check(entity.describe() == "f", "entity description must dispatch through base interface");
}

void outcomesAndCancellation() {
    using namespace xray;
    Result<history::HistoryResult> empty = history::HistoryResult{};
    check(std::holds_alternative<history::HistoryResult>(empty), "empty history is a success");
    Error missing{ErrorCode::not_found, "Missing commit", {}, "bad-oid", {}};
    Result<history::HistoryResult> failed = missing;
    check(std::get<Error>(failed).commit == "bad-oid", "error must retain context");
    auto partial = *fixtures::analysis(false);
    partial.coverage = {Completeness::partial, 1, {missing}};
    Result<analysis::AnalysisSnapshot> partialResult = partial;
    check(std::get<analysis::AnalysisSnapshot>(partialResult).coverage.skippedElements == 1,
          "partial success must retain coverage");
    Result<analysis::AnalysisSnapshot> cancelled = Cancelled{{missing}};
    check(!std::holds_alternative<analysis::AnalysisSnapshot>(cancelled), "cancelled is never a snapshot");
    std::stop_source stop;
    Progress observed;
    JobContext job{42, stop.get_token(), [&observed](const Progress& value) { observed = value; }};
    check(!job.isCancelled(), "new job must be active");
    job.report("parse", 1, 2);
    check(observed.jobId == 42 && observed.completed == 1 && observed.total == 2, "progress must carry job id");
    stop.request_stop();
    check(job.isCancelled(), "cancellation must propagate from coordinator");
    JobContext{}.report("no-listener", 0);
}

void unknownAndReviewOwnership() {
    using namespace xray;
    analysis::MetricResult unknown;
    check(!unknown.value && unknown.validity == code::Validity::unavailable, "unknown must not become zero");
    const auto report = fixtures::comparison();
    check(report.base->parsed->source->files.front().bytes->size() == 21,
          "report must keep both sources alive");
    check(report.base->analysisFingerprint == report.target->analysisFingerprint,
          "different source IDs may have compatible analysis parameters");
    check(report.items.front().sourceRefs[1].sourceId == report.target->parsed->source->id,
          "review location must identify the retained target snapshot");
    const auto history = fixtures::history();
    check(history.commits.front().parentOids.front() == history.commits.back().oid,
          "history fixture must provide first-parent order");
    check(history.commits.back().parentOids.empty(), "root commit has no parent");
    check(history.changes.front().commitOid == report.target->parsed->source->commitOid,
          "Git and analysis fixtures must describe the same revision");
    review::TrendPoint gap;
    check(!gap.value, "trend gap must not become zero");
}
} // namespace

// Check the shared ownership signature without requiring a parser implementation.
static_assert(std::is_same_v<decltype(&xray::code::parse),
    xray::Result<xray::code::ParsedSnapshot> (*)(std::shared_ptr<const xray::code::SourceSnapshot>,
        const xray::code::ParseOptions&, const xray::JobContext&)>);

int main() {
    try {
        lifetimeAndRanges();
        outcomesAndCancellation();
        unknownAndReviewOwnership();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
