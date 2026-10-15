#pragma once

#include "analysis/types.hpp"
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace xray::analysis {
// JSON stays inside the implementation; callers receive typed results.
[[nodiscard]] std::optional<Error> validateProfile(const AnalysisProfile&);
[[nodiscard]] Result<AnalysisProfile> decodeProfile(std::string_view);
[[nodiscard]] Result<std::string> encodeProfile(const AnalysisProfile&);
[[nodiscard]] Result<AnalysisProfile> loadProfile(const std::filesystem::path&);
[[nodiscard]] Result<std::filesystem::path> saveProfile(
    const AnalysisProfile&, const std::filesystem::path&);
[[nodiscard]] Result<AnalysisProfile> duplicateProfile(
    const AnalysisProfile&, std::string name);
// Metadata changes (name/id) do not invalidate measurements. Invalid settings do.
[[nodiscard]] bool isAnalysisCurrent(const AnalysisSnapshot&, const AnalysisProfile&);
} // namespace xray::analysis
