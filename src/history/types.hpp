#pragma once

#include "code/types.hpp"

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace xray::history {
using Oid = std::string; // Full resolved OID; never a moving branch name.
struct RepositorySpec { std::filesystem::path path; };
struct HistoryRequest {
    Oid startOid;
    std::size_t limit = 20;
    std::optional<std::string> relativePath;
    std::size_t parentIndex = 0; // Zero-based, checked per commit; root uses empty tree.
};
struct CommitRecord {
    Oid oid;
    std::vector<Oid> parentOids;
    std::string authorName;
    std::string authorEmail;
    std::chrono::sys_seconds authoredAt;
    std::chrono::sys_seconds committedAt;
    std::string message;
};
enum class ChangeStatus { added, deleted, modified, renamed, type_changed };
struct ChangeRecord {
    Oid commitOid;
    std::optional<Oid> parentOid;
    std::optional<std::string> oldPath;
    std::optional<std::string> newPath;
    ChangeStatus status = ChangeStatus::modified;
    std::optional<std::size_t> addedLines;
    std::optional<std::size_t> deletedLines;
    bool isBinary = false;
    bool isSymlink = false;
    bool isSubmodule = false;
    std::optional<std::string> patch;
};
struct HistoryResult {
    std::vector<CommitRecord> commits; // Newest to oldest, first-parent walk.
    std::vector<ChangeRecord> changes;
    bool hasMore = false; // Requested limit is not a read failure / partial result.
    Coverage coverage;
};
} // namespace xray::history
