#pragma once

#include "code/types.hpp"

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace xray::history {

    using Oid = std::string;

    struct RepositorySpec { std::filesystem::path path; };

    struct HistoryRequest {
        Oid startOid;
        std::size_t limit = 20;
        std::optional<std::string> relativePath;
        std::size_t parentIndex = 0;
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
        std::vector<CommitRecord> commits;
        std::vector<ChangeRecord> changes;
        bool hasMore = false;
        Coverage coverage;
    };

    class HistoryFilter {
    public:
        virtual ~HistoryFilter() = default;
        [[nodiscard]] virtual bool matches(const CommitRecord& commit) const = 0;
    };

    class AuthorFilter : public HistoryFilter {
    private:
        std::string author;
    public:
        explicit AuthorFilter(std::string a) : author(std::move(a)) {}
        [[nodiscard]] bool matches(const CommitRecord& commit) const override {
            return commit.authorName.find(author) != std::string::npos;
        }
    };

    class RevisionSelector {
    public:
        virtual ~RevisionSelector() = default;
        [[nodiscard]] virtual std::vector<CommitRecord> select(const std::vector<CommitRecord>& commits) const = 0;
    };

    class LimitSelector : public RevisionSelector {
    private:
        std::size_t limit;
    public:
        explicit LimitSelector(std::size_t n) : limit(n) {}

        [[nodiscard]] std::vector<CommitRecord> select(const std::vector<CommitRecord>& commits) const override {
            if (commits.size() <= limit) return commits;
            return std::vector<CommitRecord>(commits.begin(), commits.begin() + limit);
        }
    };

} // namespace xray::history