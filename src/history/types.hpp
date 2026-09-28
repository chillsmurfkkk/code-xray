#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include "common/job.hpp"

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
        std::chrono::sys_seconds authoredAt{};
        std::chrono::sys_seconds committedAt{};
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

    struct Coverage {
        std::unordered_map<std::string, std::vector<int>> lineCoverage;
    };

    struct HistoryResult {
        std::vector<CommitRecord> commits;
        std::vector<ChangeRecord> changes;
        bool hasMore = false;
        Coverage coverage{};
    };

    class HistoryQuery {
    public:
        virtual ~HistoryQuery() = default;
        [[nodiscard]] virtual std::vector<CommitRecord> apply(const std::vector<CommitRecord>& commits) const = 0;
    };

    class CommitRangeQuery : public HistoryQuery {
    private:
        Oid startOid;
        Oid endOid;
    public:
        CommitRangeQuery(Oid start, Oid end) : startOid(std::move(start)), endOid(std::move(end)) {}

        [[nodiscard]] std::vector<CommitRecord> apply(const std::vector<CommitRecord>& commits) const override {
            std::vector<CommitRecord> result;

            bool startFound = startOid.empty() || std::any_of(commits.begin(), commits.end(), [&](const CommitRecord& c) { return c.oid == startOid; });
            bool endFound = endOid.empty() || std::any_of(commits.begin(), commits.end(), [&](const CommitRecord& c) { return c.oid == endOid; });

            if (!startFound && !endFound) {
                return {};
            }

            bool insideRange = startOid.empty();

            for (const auto& c : commits) {
                if (!startOid.empty() && c.oid == startOid) {
                    insideRange = true;
                }

                if (insideRange) {
                    result.push_back(c);
                }

                if (!endOid.empty() && c.oid == endOid) {
                    if (!insideRange) {
                        result.clear();
                        result.push_back(c);
                    }
                    break;
                }
            }
            return result;
        }
    };

    class FileHistoryQuery : public HistoryQuery {
    private:
        std::string filePath;
        std::vector<ChangeRecord> allChanges;
    public:
        FileHistoryQuery(std::string path, std::vector<ChangeRecord> changes)
            : filePath(std::move(path)), allChanges(std::move(changes)) {
        }

        [[nodiscard]] std::vector<CommitRecord> apply(const std::vector<CommitRecord>& commits) const override {
            std::vector<CommitRecord> result;
            for (const auto& c : commits) {
                bool touchedFile = std::any_of(allChanges.begin(), allChanges.end(), [&](const ChangeRecord& ch) {
                    return ch.commitOid == c.oid &&
                        ((ch.newPath && *ch.newPath == filePath) || (ch.oldPath && *ch.oldPath == filePath));
                    });
                if (touchedFile) result.push_back(c);
            }
            return result;
        }
    };

    class RevisionSelection {
    public:
        virtual ~RevisionSelection() = default;
        [[nodiscard]] virtual std::vector<CommitRecord> select(const std::vector<CommitRecord>& commits) const = 0;
    };

    class LastNSelection : public RevisionSelection {
    private:
        std::size_t count;
    public:
        explicit LastNSelection(std::size_t n) : count(n) {}

        [[nodiscard]] std::vector<CommitRecord> select(const std::vector<CommitRecord>& commits) const override {
            if (commits.size() <= count) return commits;
            return std::vector<CommitRecord>(commits.begin(), commits.begin() + count);
        }
    };

    class ExplicitSelection : public RevisionSelection {
    private:
        std::vector<Oid> targetOids;
    public: // Виправлено доступ (було pub:)
        explicit ExplicitSelection(std::vector<Oid> oids) : targetOids(std::move(oids)) {}

        [[nodiscard]] std::vector<CommitRecord> select(const std::vector<CommitRecord>& commits) const override {
            std::vector<CommitRecord> result;
            for (const auto& c : commits) {
                if (std::find(targetOids.begin(), targetOids.end(), c.oid) != targetOids.end()) {
                    result.push_back(c);
                }
            }
            return result;
        }
    };

    class PeriodicSelection : public RevisionSelection {
    private:
        std::size_t step;
    public:
        explicit PeriodicSelection(std::size_t s) : step(s == 0 ? 1 : s) {}

        [[nodiscard]] std::vector<CommitRecord> select(const std::vector<CommitRecord>& commits) const override {
            std::vector<CommitRecord> result;
            std::size_t effectiveStep = (step == 0) ? 1 : step;
            for (std::size_t i = 0; i < commits.size(); i += effectiveStep) {
                result.push_back(commits[i]);
            }
            return result;
        }
    };

    template<typename T, typename KeyExtractor>
    std::vector<std::vector<T>> groupRecords(const std::vector<T>& records, KeyExtractor keyExtractor) {
        std::vector<std::vector<T>> groups;
        if (records.empty()) return groups;

        using KeyType = decltype(keyExtractor(records.front()));
        std::unordered_map<KeyType, std::vector<T>> mapGroups;
        std::vector<KeyType> order;

        for (const auto& item : records) {
            KeyType key = keyExtractor(item);
            if (mapGroups.find(key) == mapGroups.end()) {
                order.push_back(key);
            }
            mapGroups[key].push_back(item);
        }

        for (const auto& key : order) {
            groups.push_back(std::move(mapGroups[key]));
        }

        return groups;
    }

} // namespace xray::history