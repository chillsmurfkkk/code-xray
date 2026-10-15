#include "analysis/profiles.hpp"

#include <nlohmann/json.hpp>
#include <atomic>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace xray::analysis {
namespace {
using Json = nlohmann::json;
constexpr const char* ruleNames[] = {"long_function", "deep_nesting", "many_branches"};

Error invalid(std::string message) {
    return Error{ErrorCode::invalid_input, std::move(message), {}, {}, {}};
}
Error fileError(ErrorCode code, std::string message, const std::filesystem::path& path) {
    const auto utf8 = path.generic_u8string();
    return Error{code, std::move(message), std::string(utf8.begin(), utf8.end()), {}, {}};
}
bool validLabel(std::string_view text) {
    bool nonSpace = false;
    for (unsigned char ch : text) {
        if (ch < 32 || ch == 127) return false;
        nonSpace |= ch != ' ';
    }
    return nonSpace;
}
void requireKeys(const Json& object, std::initializer_list<const char*> names) {
    if (!object.is_object()) throw std::invalid_argument("Expected a JSON object");
    for (const auto* name : names) {
        if (!object.contains(name)) throw std::invalid_argument(std::string("Missing field: ") + name);
    }
    if (object.size() != names.size()) throw std::invalid_argument("Unknown profile field or rule");
}
std::int64_t nonnegativeInteger(const Json& value, const std::string& field) {
    if (!value.is_number_integer() ||
        (value.is_number_unsigned() && value.get<std::uint64_t>() >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))) {
        throw std::invalid_argument(field + " must be an integer in [0, INT64_MAX]");
    }
    const auto number = value.get<std::int64_t>();
    if (number < 0) throw std::invalid_argument(field + " must be nonnegative");
    return number;
}
std::string uniqueId() {
    static std::atomic<std::uint64_t> sequence{0};
    std::random_device random;
    std::ostringstream text;
    text << std::hex << std::setfill('0');
    for (int i = 0; i < 4; ++i) text << std::setw(8) << random();
    text << '-' << sequence.fetch_add(1, std::memory_order_relaxed);
    return text.str();
}
// Own an exclusively created directory, so concurrent saves never share temp files.
struct TemporaryDirectory {
    std::filesystem::path path;
    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};
} // namespace

std::optional<Error> validateProfile(const AnalysisProfile& profile) {
    if (profile.schemaVersion != 1) return invalid("Unsupported profile schemaVersion; expected 1");
    if (!validLabel(profile.id) || !validLabel(profile.name)) {
        return invalid("Profile id and name must be nonblank text without control characters");
    }
    if (profile.longFunctionLimit < 0 || profile.nestingLimit < 0 || profile.branchLimit < 0) {
        return invalid("Profile thresholds must be in [0, INT64_MAX]");
    }
    try {
        // dump validates UTF-8 supplied by C++ callers as well as imported text.
        (void)Json({profile.id, profile.name}).dump();
    } catch (const Json::exception& error) {
        return invalid(std::string("Invalid profile text: ") + error.what());
    }
    return std::nullopt;
}

Result<AnalysisProfile> decodeProfile(std::string_view text) {
    try {
        std::vector<std::set<std::string>> keys;
        auto rejectDuplicates = [&keys](int, Json::parse_event_t event, Json& parsed) {
            if (event == Json::parse_event_t::object_start) keys.emplace_back();
            if (event == Json::parse_event_t::key &&
                !keys.back().insert(parsed.get<std::string>()).second) {
                throw std::invalid_argument("Duplicate JSON field: " + parsed.get<std::string>());
            }
            if (event == Json::parse_event_t::object_end) keys.pop_back();
            return true;
        };
        const auto json = Json::parse(text.begin(), text.end(), rejectDuplicates);
        requireKeys(json, {"schemaVersion", "id", "name", "rules"});
        if (nonnegativeInteger(json.at("schemaVersion"), "schemaVersion") != 1) {
            return invalid("Unsupported profile schemaVersion; expected 1");
        }
        if (!json.at("id").is_string() || !json.at("name").is_string()) {
            return invalid("Profile id and name must be strings");
        }
        const auto& rules = json.at("rules");
        requireKeys(rules, {ruleNames[0], ruleNames[1], ruleNames[2]});
        AnalysisProfile profile;
        profile.id = json.at("id").get<std::string>();
        profile.name = json.at("name").get<std::string>();
        std::int64_t* thresholds[] = {&profile.longFunctionLimit, &profile.nestingLimit, &profile.branchLimit};
        for (std::size_t i = 0; i < profile.enabledRules.size(); ++i) {
            const auto& rule = rules.at(ruleNames[i]);
            requireKeys(rule, {"enabled", "threshold"});
            if (!rule.at("enabled").is_boolean()) {
                return invalid(std::string(ruleNames[i]) + ".enabled must be boolean");
            }
            profile.enabledRules[i] = rule.at("enabled").get<bool>();
            *thresholds[i] = nonnegativeInteger(rule.at("threshold"), std::string(ruleNames[i]) + ".threshold");
        }
        if (auto error = validateProfile(profile)) return *error;
        return profile;
    } catch (const Json::exception& error) {
        return invalid(std::string("Invalid JSON profile: ") + error.what());
    } catch (const std::invalid_argument& error) {
        return invalid(error.what());
    }
}

Result<std::string> encodeProfile(const AnalysisProfile& profile) {
    if (auto error = validateProfile(profile)) return *error;
    Json rules = Json::object();
    const std::int64_t thresholds[] = {profile.longFunctionLimit, profile.nestingLimit, profile.branchLimit};
    for (std::size_t i = 0; i < profile.enabledRules.size(); ++i) {
        rules[ruleNames[i]] = {{"enabled", profile.enabledRules[i]}, {"threshold", thresholds[i]}};
    }
    return Json{{"schemaVersion", profile.schemaVersion}, {"id", profile.id},
                {"name", profile.name}, {"rules", rules}}.dump(2) + "\n";
}

Result<AnalysisProfile> loadProfile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return fileError(ErrorCode::read_error, "Cannot open profile", path);
    const std::string bytes((std::istreambuf_iterator<char>(input)), {});
    if (input.bad()) return fileError(ErrorCode::read_error, "Cannot read profile", path);
    auto result = decodeProfile(bytes);
    if (auto* error = std::get_if<Error>(&result)) {
        *error = fileError(error->code, error->message, path);
    }
    return result;
}

Result<std::filesystem::path> saveProfile(const AnalysisProfile& profile, const std::filesystem::path& path) {
    auto encoded = encodeProfile(profile);
    if (auto* error = std::get_if<Error>(&encoded)) return *error;
    if (path.empty() || !path.has_filename()) return invalid("Profile destination must name a file");
    const auto parent = path.has_parent_path() ? path.parent_path() : std::filesystem::path(".");
    auto tempPath = parent / (".xray-profile-" + uniqueId());
    std::error_code ec;
    if (!std::filesystem::create_directory(tempPath, ec)) {
        return fileError(ErrorCode::export_error, "Cannot create temporary profile directory", path);
    }
    TemporaryDirectory cleanup{tempPath};
    const auto temporary = tempPath / "profile.json";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    const auto& bytes = std::get<std::string>(encoded);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close();
    if (!output) return fileError(ErrorCode::export_error, "Cannot write complete profile", path);
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return fileError(ErrorCode::export_error, "Cannot replace profile", path);
    }
#else
    std::filesystem::rename(temporary, path, ec);
    if (ec) return fileError(ErrorCode::export_error, "Cannot replace profile: " + ec.message(), path);
#endif
    return path;
}

Result<AnalysisProfile> duplicateProfile(const AnalysisProfile& original, std::string name) {
    if (auto error = validateProfile(original)) return *error;
    auto copy = original;
    copy.id = "profile-" + uniqueId();
    copy.name = std::move(name);
    if (auto error = validateProfile(copy)) return *error;
    return copy;
}

bool isAnalysisCurrent(const AnalysisSnapshot& snapshot, const AnalysisProfile& profile) {
    if (validateProfile(profile) || validateProfile(snapshot.profile)) return false;
    const auto& old = snapshot.profile;
    return old.schemaVersion == profile.schemaVersion && old.longFunctionLimit == profile.longFunctionLimit &&
        old.nestingLimit == profile.nestingLimit && old.branchLimit == profile.branchLimit &&
        old.enabledRules == profile.enabledRules;
}
} // namespace xray::analysis
