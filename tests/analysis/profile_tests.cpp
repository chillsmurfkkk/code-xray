#include "analysis/profiles.hpp"
#include "analysis/rules.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace xray;
using namespace xray::analysis;
using Json = nlohmann::json;
namespace {
void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
template<class T> const T& success(const Result<T>& result) {
    if (const auto* error = std::get_if<Error>(&result)) throw std::runtime_error(error->message);
    check(std::holds_alternative<T>(result), "Unexpected cancellation");
    return std::get<T>(result);
}
std::string encode(const AnalysisProfile& p = {}) { return success(encodeProfile(p)); }
void rejects(const Json& json) {
    auto result = decodeProfile(json.dump());
    check(std::holds_alternative<Error>(result), "Malformed profile must be rejected");
    check(!std::get<Error>(result).message.empty(), "Error needs an explanation");
}
void schemaAndRoundTrip() {
    AnalysisProfile original;
    original.name = "Мій профіль";
    original.longFunctionLimit = std::numeric_limits<std::int64_t>::max();
    original.nestingLimit = 0;
    original.enabledRules = {false, true, false};
    auto encoded = encode(original);
    auto decoded = decodeProfile(encoded);
    check(encode(success(decoded)) == encoded, "Round trip must preserve all fields and stable encoding");
    auto json = Json::parse(encoded);
    for (const auto& key : {"schemaVersion", "id", "name", "rules"}) {
        auto changed = json;
        changed.erase(key);
        rejects(changed);
    }
    for (const auto& version : {Json(0), Json(2), Json(-1), Json(1.0), Json(true), Json("1"), Json(nullptr)}) {
        auto changed = json; changed["schemaVersion"] = version; rejects(changed);
    }
    for (const auto& rule : {"long_function", "deep_nesting", "many_branches"}) {
        auto changed = json; changed["rules"].erase(rule); rejects(changed);
        for (const auto& bad : {Json(-1), Json(3.5), Json(true), Json("3"), Json(nullptr),
                                Json(std::numeric_limits<std::uint64_t>::max())}) {
            changed = json; changed["rules"][rule]["threshold"] = bad; rejects(changed);
        }
        changed = json; changed["rules"][rule]["enabled"] = 1; rejects(changed);
        changed = json; changed["rules"][rule]["unknown"] = 5; rejects(changed);
    }
    auto changed = json; changed["rules"]["unknown_rule"] = {}; rejects(changed);
    changed = json; changed["unknown"] = 1; rejects(changed);
    changed = json; changed["name"] = "   "; rejects(changed);
    changed = json; changed["id"] = ""; rejects(changed);
    changed = json; changed["name"] = 42; rejects(changed);
    changed = json; changed["name"] = "bad\nname"; rejects(changed);
    for (const auto& bad : {"{", "[]", "null", "{} trailing", "{\"id\":1,\"id\":2}"}) {
        check(std::holds_alternative<Error>(decodeProfile(bad)), "Invalid/duplicate JSON must fail");
    }
    auto duplicated = encoded;
    const auto pos = duplicated.find("\"enabled\": false");
    duplicated.insert(pos, "\"enabled\": true, ");
    check(std::holds_alternative<Error>(decodeProfile(duplicated)), "Nested duplicate keys must fail");
    original.name = std::string(1, static_cast<char>(0xff));
    check(std::holds_alternative<Error>(encodeProfile(original)), "Invalid UTF-8 must fail serialization");
    original = {}; original.branchLimit = -1;
    check(std::holds_alternative<Error>(encodeProfile(original)), "Invalid C++ profile must fail serialization");
}
void allRulesThroughBaseInterface() {
    std::vector<std::unique_ptr<Rule>> rules;
    rules.push_back(std::make_unique<LongFunctionRule>());
    rules.push_back(std::make_unique<DeepNestingRule>());
    rules.push_back(std::make_unique<ManyBranchesRule>());
    const char* units[] = {"lines", "nesting levels", "branches"};
    const std::int64_t limits[] = {60, 3, 10};
    AnalysisProfile profile;
    code::FunctionEntity fn;
    fn.id = "fn"; fn.name = "run"; fn.relativePath = "src/test.cpp";
    fn.bodyRange = code::SourceRange{};
    fn.validity = code::Validity::valid;
    for (std::size_t i = 0; i < rules.size(); ++i) {
        const Rule& rule = *rules[i];
        MetricResult metric{fn.id, rule.targetMetricId(), limits[i], code::Validity::valid, {}};
        check(!rule.evaluate(fn, metric, profile), "Equality is not a violation");
        metric.value = limits[i] + 1;
        auto finding = rule.evaluate(fn, metric, profile);
        check(finding && finding->ruleId == rule.id() && finding->threshold == limits[i] &&
              finding->observedValue == limits[i] + 1, "Threshold must produce typed finding");
        check(finding->explanation.find(units[i]) != std::string::npos &&
              finding->explanation.find("src/test.cpp:1") != std::string::npos, "Explanation must include unit and location");
        profile.enabledRules[i] = false;
        check(!rule.evaluate(fn, metric, profile), "Disabled rule must not run");
        profile.enabledRules[i] = true;
        metric.validity = code::Validity::syntactic_only;
        finding = rule.evaluate(fn, metric, profile);
        check(finding && finding->validity == code::Validity::syntactic_only &&
              finding->explanation.find("syntactic-only") != std::string::npos, "Precision warning must survive");
        metric.validity = code::Validity::valid;
        fn.validity = code::Validity::syntactic_only;
        check(rule.evaluate(fn, metric, profile)->validity == code::Validity::syntactic_only,
              "Function precision must survive");
        fn.validity = code::Validity::unavailable;
        check(!rule.evaluate(fn, metric, profile), "Unavailable function must not trigger a rule");
        fn.validity = code::Validity::valid;
        for (auto validity : {code::Validity::unavailable, code::Validity::not_applicable}) {
            metric.validity = validity;
            check(!rule.evaluate(fn, metric, profile), "Unavailable metric must not trigger a rule");
        }
        metric.validity = code::Validity::valid;
        metric.value.reset();
        check(!rule.evaluate(fn, metric, profile), "Missing value must not become zero");
        metric.value = limits[i] + 1;
        metric.entityId = "other";
        check(!rule.evaluate(fn, metric, profile), "Metric must belong to this function");
        metric.entityId = fn.id;
        const auto body = fn.bodyRange;
        fn.bodyRange.reset();
        check(!rule.evaluate(fn, metric, profile), "Declaration must not trigger a rule");
        fn.bodyRange = body;
        auto custom = profile;
        std::int64_t* customLimits[] = {&custom.longFunctionLimit, &custom.nestingLimit, &custom.branchLimit};
        *customLimits[i] = limits[i] + 1;
        check(!rule.evaluate(fn, metric, custom), "Custom threshold equality must not trigger");
        ++*metric.value;
        check(rule.evaluate(fn, metric, custom).has_value(), "Custom threshold excess must trigger");
        metric.metricId = static_cast<MetricId>((i + 1) % 3);
        check(!rule.evaluate(fn, metric, profile), "Wrong metric must be ignored");
    }
    profile.nestingLimit = 0;
    MetricResult metric{fn.id, MetricId::max_nesting, 1, code::Validity::valid, {}};
    check(rules[1]->evaluate(fn, metric, profile).has_value(), "Custom threshold must be used");
}
void copyingAndStaleness() {
    AnalysisSnapshot snapshot;
    auto current = snapshot.profile;
    check(isAnalysisCurrent(snapshot, current), "Initial profile is current");
    auto copyResult = duplicateProfile(current, "Copy");
    auto copy = success(copyResult);
    check(copy.id != current.id && copy.name == "Copy", "Copy needs its own identity");
    check(isAnalysisCurrent(snapshot, copy), "Metadata does not change analysis settings");
    auto secondCopy = duplicateProfile(current, "Another copy");
    check(success(secondCopy).id != copy.id, "Copies need different IDs");
    copy.nestingLimit++;
    check(!isAnalysisCurrent(snapshot, copy) && current.nestingLimit == 3, "Copy must be independent and invalidate old analysis");
    for (int i = 0; i < 3; ++i) {
        auto changed = current; changed.enabledRules[i] = false;
        check(!isAnalysisCurrent(snapshot, changed), "Rule toggle must mark old analysis stale");
    }
    auto changed = current; changed.longFunctionLimit++;
    check(!isAnalysisCurrent(snapshot, changed), "Length threshold change must invalidate");
    changed = current; changed.branchLimit++;
    check(!isAnalysisCurrent(snapshot, changed), "Branch threshold change must invalidate");
    check(std::holds_alternative<Error>(duplicateProfile(current, "")), "Empty copy name must fail");
    changed = current; changed.schemaVersion = 2;
    check(!isAnalysisCurrent(snapshot, changed), "Invalid profile is never current");
}
void fileOperations() {
    auto copy = duplicateProfile({}, "file test");
    const auto root = std::filesystem::temp_directory_path() / success(copy).id;
    check(std::filesystem::create_directory(root), "Test directory must be fresh");
    struct Cleanup {
        std::filesystem::path root;
        ~Cleanup() { std::error_code ec; std::filesystem::remove_all(root, ec); }
    } cleanup{root};
    const auto path = root / std::filesystem::path(std::u8string(u8"профіль.json"));
    AnalysisProfile active;
    success(saveProfile(active, path));
    auto loaded = loadProfile(path);
    check(encode(success(loaded)) == encode(active), "Saved profile must load unchanged");
    auto updated = active; updated.branchLimit = 5;
    success(saveProfile(updated, path));
    check(encode(success(loadProfile(path))) == encode(updated), "Existing profile must be replaceable");
    auto invalid = active; invalid.schemaVersion = 2;
    check(std::holds_alternative<Error>(saveProfile(invalid, path)), "Invalid save must fail");
    check(encode(success(loadProfile(path))) == encode(updated), "Failed save must retain original file");
    const auto blocked = root / "directory.json";
    std::filesystem::create_directory(blocked);
    std::ofstream(blocked / "keep.txt") << "old data";
    check(std::holds_alternative<Error>(saveProfile(active, blocked)), "Directory destination must fail");
    check(std::filesystem::exists(blocked / "keep.txt"), "Replacement failure must preserve existing destination");
    check(std::holds_alternative<Error>(saveProfile(active, root / "missing" / "p.json")), "Missing parent must fail");
    std::ofstream(path, std::ios::trunc) << "{ broken";
    auto failed = loadProfile(path);
    check(std::holds_alternative<Error>(failed) && std::get<Error>(failed).path.has_value(), "Import error must include path");
    // Application applies a profile only after load succeeds.
    if (const auto* profile = std::get_if<AnalysisProfile>(&failed)) active = *profile;
    check(encode(active) == encode(), "Failed import must preserve the active profile");
    check(std::holds_alternative<Error>(loadProfile(root / "absent.json")), "Missing input must fail");
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        check(entry.path().filename().string().find(".xray-profile-") != 0, "Temporary files must be cleaned after success/failure");
    }
}
} // namespace
int main() {
    try {
        schemaAndRoundTrip(); allRulesThroughBaseInterface(); copyingAndStaleness(); fileOperations();
        std::cout << "All profile and rule tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
