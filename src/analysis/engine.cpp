#include "analysis/engine.hpp"
#include "analysis/profiles.hpp"
#include <algorithm>
#include <set>
#include <tuple>

namespace xray::analysis {
namespace {
Error invalid(std::string message) {
    return {ErrorCode::invalid_input, std::move(message), {}, {}, {}};
}
std::vector<const code::FunctionEntity*> functions(const code::ParsedSnapshot& parsed) {
    std::vector<const code::FunctionEntity*> result;
    for (const auto& unit : parsed.units)
        for (const auto& fn : unit.functions) if (fn) result.push_back(fn.get());
    return result;
}
std::map<std::string, const code::FunctionEntity*> functionIndex(const AnalysisSnapshot& snapshot) {
    std::map<std::string, const code::FunctionEntity*> result;
    if (snapshot.parsed) for (auto* fn : functions(*snapshot.parsed)) result.emplace(fn->id, fn);
    return result;
}
std::int64_t limitFor(const AnalysisProfile& p, RuleId rule) {
    switch (rule) {
        case RuleId::long_function: return p.longFunctionLimit;
        case RuleId::deep_nesting: return p.nestingLimit;
        case RuleId::many_branches: return p.branchLimit;
    }
    return -1;
}
} // namespace

Result<MetricRegistry> buildMetricRegistry() {
    MetricRegistry registry;
    registry.push_back(std::make_unique<LengthMetric>());
    registry.push_back(std::make_unique<NestingMetric>());
    registry.push_back(std::make_unique<BranchCountMetric>());
    std::set<MetricId> ids;
    for (const auto& metric : registry) {
        if (!metric || !ids.insert(metric->id()).second) return invalid("Duplicate/null metric registration");
    }
    if (ids != std::set<MetricId>{MetricId::function_lines, MetricId::max_nesting, MetricId::branch_count})
        return invalid("Required metric missing from registry");
    return registry;
}
Result<RuleRegistry> resolveEnabledRules(const AnalysisProfile& profile, const MetricRegistry& metrics) {
    if (auto error = validateProfile(profile)) return *error;
    RuleRegistry all, enabled;
    all.push_back(std::make_unique<LongFunctionRule>());
    all.push_back(std::make_unique<DeepNestingRule>());
    all.push_back(std::make_unique<ManyBranchesRule>());
    for (auto& rule : all) {
        if (!profile.enabledRules[static_cast<std::size_t>(rule->id())]) continue;
        const auto dependency = std::find_if(metrics.begin(), metrics.end(), [&](const auto& metric) {
            return metric && metric->id() == rule->targetMetricId();
        });
        if (dependency == metrics.end()) return invalid("Enabled rule has no registered metric");
        enabled.push_back(std::move(rule));
    }
    return enabled;
}

Result<std::string> makeAnalysisFingerprint(const code::ParsedSnapshot& parsed, const AnalysisProfile& profile) {
    if (!parsed.source) return invalid("Fingerprint requires source snapshot");
    if (parsed.options.grammarVersion.empty() || parsed.options.structureVersion.empty())
        return invalid("Parser and structure versions must be nonempty");
    if (auto error = validateProfile(profile)) return *error;
    std::string result;
    auto field = [&](std::string_view value) { result += std::to_string(value.size()) + ":" + std::string(value); };
    field("analysis-v1"); field("metrics-v1"); field("rules-v1");
    field(parsed.options.grammarVersion); field(parsed.options.structureVersion);
    auto selection = [&](std::vector<std::string> values) {
        std::sort(values.begin(), values.end());
        values.erase(std::unique(values.begin(), values.end()), values.end());
        field(std::to_string(values.size()));
        for (const auto& value : values) field(value);
    };
    selection(parsed.source->selection.extensions);
    selection(parsed.source->selection.excludedDirectories);
    selection(parsed.source->selection.relativePaths);
    field(std::to_string(profile.schemaVersion));
    for (auto rule : {RuleId::long_function, RuleId::deep_nesting, RuleId::many_branches}) {
        field(std::to_string(limitFor(profile, rule)));
        field(profile.enabledRules[static_cast<std::size_t>(rule)] ? "1" : "0");
    }
    // The canonical representation itself is the fingerprint; no unstable std::hash.
    return result;
}

CoverageSummary summarizeCoverage(const code::ParsedSnapshot& parsed) {
    CoverageSummary summary;
    summary.coverage = parsed.coverage;
    for (auto* fn : functions(parsed)) {
        if (fn->validity == code::Validity::unavailable) {
            ++summary.unavailable;
            summary.coverage.completeness = Completeness::partial;
        } else if (!fn->bodyRange || fn->validity == code::Validity::not_applicable) ++summary.notApplicable;
        else if (fn->validity == code::Validity::syntactic_only) ++summary.syntacticOnly;
        else ++summary.valid;
    }
    return summary;
}

Result<AnalysisSnapshot> analyze(std::shared_ptr<const code::ParsedSnapshot> parsed,
                                 const AnalysisProfile& profile, const JobContext& job) {
    if (job.isCancelled()) return Cancelled{};
    if (!parsed || !parsed->source || parsed->source->id.empty()) return invalid("Analysis requires a named source snapshot");
    auto fingerprint = makeAnalysisFingerprint(*parsed, profile);
    if (const auto* error = std::get_if<Error>(&fingerprint)) return *error;
    std::set<std::string> ids;
    for (const auto& unit : parsed->units) {
        for (const auto& fn : unit.functions) {
            if (job.isCancelled()) return Cancelled{parsed->coverage.diagnostics};
            if (!fn || fn->id.empty() || !ids.insert(fn->id).second) return invalid("Null function or empty/duplicate entity ID");
        }
    }
    auto metricsResult = buildMetricRegistry();
    if (auto* error = std::get_if<Error>(&metricsResult)) return *error;
    auto metrics = std::move(std::get<MetricRegistry>(metricsResult));
    auto rulesResult = resolveEnabledRules(profile, metrics);
    if (auto* error = std::get_if<Error>(&rulesResult)) return *error;
    auto rules = std::move(std::get<RuleRegistry>(rulesResult));
    AnalysisSnapshot snapshot;
    snapshot.parsed = std::move(parsed);
    snapshot.profile = profile;
    snapshot.analysisFingerprint = std::get<std::string>(std::move(fingerprint));
    snapshot.coverage = summarizeCoverage(*snapshot.parsed).coverage;
    // Source plus parameter representation identifies an analysis deterministically.
    snapshot.id = std::to_string(snapshot.parsed->source->id.size()) + ":" + snapshot.parsed->source->id + snapshot.analysisFingerprint;
    const auto inputs = functions(*snapshot.parsed);
    job.report("analyze-functions", 0, inputs.size());
    std::size_t completed = 0;
    for (auto* fn : inputs) {
        if (job.isCancelled()) return Cancelled{snapshot.coverage.diagnostics};
        for (const auto& implementation : metrics) {
            const Metric& metric = *implementation;
            auto measured = metric.calculate(*fn);
            for (const auto& implementationRule : rules) {
                const Rule& rule = *implementationRule;
                if (rule.targetMetricId() == measured.metricId) {
                    if (auto finding = rule.evaluate(*fn, measured, snapshot.profile)) snapshot.findings.push_back(std::move(*finding));
                }
            }
            snapshot.metrics.push_back(std::move(measured));
        }
        job.report("analyze-functions", ++completed, inputs.size());
    }
    if (job.isCancelled()) return Cancelled{snapshot.coverage.diagnostics};
    return snapshot;
}

std::vector<Finding> filterFindings(const AnalysisSnapshot& snapshot, const FindingFilter& filter) {
    const auto index = functionIndex(snapshot);
    std::vector<Finding> result;
    for (const auto& finding : snapshot.findings) {
        const auto fn = index.find(finding.entityId);
        if (fn == index.end()) continue;
        if (filter.rule && finding.ruleId != *filter.rule) continue;
        if (filter.validity && finding.validity != *filter.validity) continue;
        if (fn->second->relativePath.find(filter.pathContains) == std::string::npos) continue;
        if (finding.explanation.find(filter.textContains) == std::string::npos &&
            fn->second->describe().find(filter.textContains) == std::string::npos) continue;
        result.push_back(finding);
    }
    return result;
}
std::map<std::string, std::vector<Finding>> groupFindingsByFile(const AnalysisSnapshot& snapshot) {
    const auto index = functionIndex(snapshot);
    std::map<std::string, std::vector<Finding>> groups;
    for (const auto& finding : snapshot.findings) {
        const auto fn = index.find(finding.entityId);
        if (fn != index.end()) groups[fn->second->relativePath].push_back(finding);
    }
    for (auto& [path, findings] : groups) {
        (void)path;
        std::sort(findings.begin(), findings.end(), [](const auto& a, const auto& b) {
            return std::tie(a.entityId, a.ruleId, a.id) < std::tie(b.entityId, b.ruleId, b.id);
        });
    }
    return groups;
}
std::vector<code::EntityId> rankFunctions(const AnalysisSnapshot& snapshot, MetricId id, bool descending) {
    const auto index = functionIndex(snapshot);
    std::map<std::string, std::optional<std::int64_t>> values;
    std::vector<std::string> ranked;
    for (const auto& [entity, fn] : index) { (void)fn; ranked.push_back(entity); }
    for (const auto& metric : snapshot.metrics) {
        if (metric.metricId == id && (metric.validity == code::Validity::valid || metric.validity == code::Validity::syntactic_only))
            values[metric.entityId] = metric.value;
    }
    std::sort(ranked.begin(), ranked.end(), [&](const auto& a, const auto& b) {
        const auto va = values[a], vb = values[b];
        if (va.has_value() != vb.has_value()) return va.has_value();
        if (va && vb && va != vb) return descending ? *va > *vb : *va < *vb;
        return std::tie(index.at(a)->relativePath, a) < std::tie(index.at(b)->relativePath, b);
    });
    return ranked;
}

std::vector<Error> validateAnalysisResult(const AnalysisSnapshot& snapshot) {
    std::vector<Error> errors;
    auto fail = [&](std::string message) { errors.push_back(invalid(std::move(message))); };
    if (!snapshot.parsed || !snapshot.parsed->source) { fail("Missing parsed/source snapshot"); return errors; }
    if (snapshot.id.empty()) fail("Empty analysis ID");
    if (snapshot.parsed->source->id.empty()) fail("Empty source ID");
    auto fingerprint = makeAnalysisFingerprint(*snapshot.parsed, snapshot.profile);
    if (std::holds_alternative<Error>(fingerprint) || snapshot.analysisFingerprint.empty() ||
        (std::holds_alternative<std::string>(fingerprint) && std::get<std::string>(fingerprint) != snapshot.analysisFingerprint)) fail("Invalid analysis fingerprint");
    if (snapshot.metricSchemaVersion != "metrics-v1") fail("Unknown metric schema version");
    const auto index = functionIndex(snapshot);
    std::size_t expectedFunctions = 0;
    for (const auto& unit : snapshot.parsed->units) for (const auto& fn : unit.functions) {
        ++expectedFunctions;
        if (!fn || fn->id.empty()) fail("Null function or empty ID");
    }
    if (expectedFunctions != index.size()) fail("Duplicate/null function entities");
    std::map<std::pair<std::string, MetricId>, const MetricResult*> measured;
    for (const auto& metric : snapshot.metrics) {
        if (!index.contains(metric.entityId)) { fail("Metric references unknown entity"); continue; }
        if (static_cast<unsigned>(metric.metricId) > 2) fail("Unknown metric ID");
        if (!measured.emplace(std::make_pair(metric.entityId, metric.metricId), &metric).second) fail("Duplicate metric");
        const auto* fn = index.at(metric.entityId);
        const auto validity = fn->validity == code::Validity::unavailable ? code::Validity::unavailable :
            (!fn->bodyRange ? code::Validity::not_applicable : fn->validity);
        if (metric.validity != validity) fail("Metric validity disagrees with source function");
        const bool numeric = metric.validity == code::Validity::valid || metric.validity == code::Validity::syntactic_only;
        if (numeric != metric.value.has_value() || (metric.value && *metric.value < 0)) fail("Invalid metric value/validity");
    }
    for (const auto& [entity, fn] : index) {
        (void)fn;
        for (auto id : {MetricId::function_lines, MetricId::max_nesting, MetricId::branch_count})
            if (!measured.contains({entity, id})) fail("Missing required metric");
    }
    std::set<std::string> findingIds;
    std::set<std::pair<std::string, RuleId>> findingKeys;
    for (const auto& finding : snapshot.findings) {
        if (finding.id.empty() || !findingIds.insert(finding.id).second ||
            !findingKeys.insert({finding.entityId, finding.ruleId}).second) fail("Empty/duplicate finding");
        if (static_cast<unsigned>(finding.ruleId) > 2) { fail("Unknown rule ID"); continue; }
        const auto metric = measured.find({finding.entityId, static_cast<MetricId>(finding.ruleId)});
        if (metric == measured.end() || !metric->second->value ||
            (metric->second->validity != code::Validity::valid && metric->second->validity != code::Validity::syntactic_only) ||
            !snapshot.profile.enabledRules[static_cast<std::size_t>(finding.ruleId)] ||
            finding.observedValue != *metric->second->value || finding.threshold != limitFor(snapshot.profile, finding.ruleId) ||
            finding.observedValue <= finding.threshold || finding.validity != metric->second->validity || finding.explanation.empty()) fail("Finding disagrees with its rule/metric");
    }
    // Check completeness as well as checking each existing finding.
    std::size_t expectedFindings = 0;
    for (const auto& [key, metric] : measured) {
        const auto rule = static_cast<RuleId>(key.second);
        if (static_cast<unsigned>(rule) <= 2 && snapshot.profile.enabledRules[static_cast<std::size_t>(rule)] &&
            metric->value && (metric->validity == code::Validity::valid || metric->validity == code::Validity::syntactic_only) &&
            *metric->value > limitFor(snapshot.profile, rule)) ++expectedFindings;
    }
    if (expectedFindings != snapshot.findings.size()) fail("Missing or extra findings");
    const auto coverage = summarizeCoverage(*snapshot.parsed).coverage;
    if (snapshot.coverage.completeness != coverage.completeness || snapshot.coverage.skippedElements != coverage.skippedElements ||
        snapshot.coverage.diagnostics.size() != coverage.diagnostics.size()) fail("Coverage was not inherited");
    return errors;
}
} // namespace xray::analysis
