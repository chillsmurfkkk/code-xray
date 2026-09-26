#pragma once

#include "analysis/types.hpp"
#include "history/types.hpp"
#include "review/types.hpp"
#include <utility>

namespace fixtures {
// Hand-authored contracts, not output from a parser/metric/comparison implementation.
inline std::shared_ptr<const xray::analysis::AnalysisSnapshot> analysis(bool after) {
    using namespace xray;
    const std::string text = after ? "int f() {\n    return 1;\n}" : "int f() { return 1; }";
    auto source = std::make_shared<code::SourceSnapshot>();
    source->id = after ? "fixture-after" : "fixture-before";
    source->kind = code::SourceKind::git;
    source->commitOid = std::string(40, after ? 'b' : 'a');
    source->files.push_back({"src/example.cpp", std::make_shared<const std::string>(text),
                             after ? "fixture-content-after" : "fixture-content-before"});

    auto function = std::make_shared<code::FunctionEntity>();
    function->id = "function-1";
    function->name = "f";
    function->relativePath = "src/example.cpp";
    // Length-prefixed fields/tokens: unambiguous even if a filename contains delimiters.
    function->identityKey = "15:src/example.cpp1:f3:int1:f1:(1:)";
    function->signatureTokens = {"int", "f", "(", ")"};
    function->range = {0, text.size(), 1, after ? 3u : 1u};
    function->bodyRange = code::SourceRange{8, text.size(), 1, after ? 3u : 1u};
    function->controlTree.range = *function->bodyRange;

    auto file = std::make_shared<code::FileEntity>();
    file->id = "file-1";
    file->name = "example.cpp";
    file->relativePath = "src/example.cpp";
    file->sourceBytes = source->files.front().bytes;
    file->contentId = source->files.front().contentId;
    file->range = function->range;
    file->children.push_back(function);

    auto parsed = std::make_shared<code::ParsedSnapshot>();
    parsed->source = source;
    parsed->units.push_back({"src/example.cpp", file, {function}, {}, {}});
    auto result = std::make_shared<analysis::AnalysisSnapshot>();
    result->id = after ? "analysis-after" : "analysis-before";
    result->parsed = parsed;
    result->analysisFingerprint = "fixture-compatible-v1";
    result->metrics = {
        {function->id, analysis::MetricId::function_lines, after ? 3 : 1, code::Validity::valid, {}},
        {function->id, analysis::MetricId::max_nesting, 0, code::Validity::valid, {}},
        {function->id, analysis::MetricId::branch_count, 0, code::Validity::valid, {}}
    };
    return result;
}

inline xray::history::HistoryResult history() {
    using namespace xray;
    history::CommitRecord root;
    root.oid = std::string(40, 'a');
    root.authorName = "Fixture author";
    root.message = "Add example";
    history::CommitRecord next = root;
    next.oid = std::string(40, 'b');
    next.parentOids = {root.oid};
    next.message = "Expand function to three lines";
    history::ChangeRecord change;
    change.commitOid = next.oid;
    change.parentOid = root.oid;
    change.oldPath = "src/example.cpp";
    change.newPath = "src/example.cpp";
    change.addedLines = 3;
    change.deletedLines = 1;
    return {{next, root}, {change}, false, {}};
}

inline xray::review::ComparisonReport comparison() {
    using namespace xray;
    review::ComparisonReport report;
    report.base = analysis(false);
    report.target = analysis(true);
    report.baseId = report.base->id;
    report.targetId = report.target->id;
    report.analysisFingerprint = report.base->analysisFingerprint;
    report.metricSchemaVersion = report.base->metricSchemaVersion;
    const auto& before = *report.base->parsed->units.front().functions.front();
    const auto& after = *report.target->parsed->units.front().functions.front();
    review::ReviewItem item;
    item.id = "fixture-length-change";
    item.kind = review::ReviewKind::changed;
    item.entityKey = before.identityKey;
    item.before = before.id;
    item.after = after.id;
    item.confidence = review::Confidence::exact;
    item.explanation = "Function length increased from 1 to 3 lines.";
    item.metrics.push_back({analysis::MetricId::function_lines, 1, 3, 2});
    item.sourceRefs = {
        {report.base->parsed->source->id, before.relativePath, before.id, before.range},
        {report.target->parsed->source->id, after.relativePath, after.id, after.range}
    };
    report.items.push_back(std::move(item));
    return report;
}
} // namespace fixtures
