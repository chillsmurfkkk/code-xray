#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>
#include <cctype>

#include "common/job.hpp"
#include "code/types.hpp"

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

    struct HistoryResult {
        std::vector<CommitRecord> commits;
        std::vector<ChangeRecord> changes;
        bool hasMore = false;
        xray::Coverage coverage{};
    };

    // Helper function for case-insensitive comparison
    inline std::string toLowerCopy(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
            });
        return s;
    }

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
            bool startExists = startOid.empty() || std::any_of(commits.begin(), commits.end(), [&](const CommitRecord& c) { return c.oid == startOid; });
            bool endExists = endOid.empty() || std::any_of(commits.begin(), commits.end(), [&](const CommitRecord& c) { return c.oid == endOid; });

            if (!startExists) {
                throw std::invalid_argument("CommitRangeQuery: start boundary OID not found: " + startOid);
            }
            if (!endExists) {
                throw std::invalid_argument("CommitRangeQuery: end boundary OID not found: " + endOid);
            }

            if (commits.empty()) {
                return {};
            }

            if (!startOid.empty() && !endOid.empty() && startOid != endOid) {
                auto startIt = std::find_if(commits.begin(), commits.end(), [&](const CommitRecord& c) { return c.oid == startOid; });
                auto endIt = std::find_if(commits.begin(), commits.end(), [&](const CommitRecord& c) { return c.oid == endOid; });

                if (std::distance(commits.begin(), startIt) > std::distance(commits.begin(), endIt)) {
                    throw std::invalid_argument("CommitRangeQuery: inverted boundary order (start OID appears after end OID)");
                }
            }

            std::vector<CommitRecord> result;
            bool insideRange = startOid.empty();

            for (const auto& c : commits) {
                if (!startOid.empty() && c.oid == startOid) {
                    insideRange = true;
                }

                if (insideRange) {
                    result.push_back(c);
                }

                if (!endOid.empty() && c.oid == endOid) {
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
        bool strictMatch;
    public:
        FileHistoryQuery(std::string path, std::vector<ChangeRecord> changes, bool strict = true)
            : filePath(std::move(path)), allChanges(std::move(changes)), strictMatch(strict) {
            if (filePath.empty()) {
                throw std::invalid_argument("FileHistoryQuery: file path cannot be empty");
            }
        }

        [[nodiscard]] std::vector<CommitRecord> apply(const std::vector<CommitRecord>& commits) const override {
            std::vector<CommitRecord> result;
            for (const auto& c : commits) {
                bool touchedFile = std::any_of(allChanges.begin(), allChanges.end(), [&](const ChangeRecord& ch) {
                    if (ch.commitOid != c.oid) return false;
                    auto matchPath = [&](const std::optional<std::string>& p) {
                        if (!p) return false;
                        if (strictMatch) {
                            return *p == filePath;
                        }
                        else {
                            return p->find(filePath) != std::string::npos;
                        }
                        };
                    return matchPath(ch.newPath) || matchPath(ch.oldPath);
                    });
                if (touchedFile) {
                    result.push_back(c);
                }
            }
            return result;
        }
    };

    class AuthorHistoryQuery : public HistoryQuery {
    private:
        std::string targetAuthor;
        bool caseSensitive;
        std::size_t maxResults;
    public:
        AuthorHistoryQuery(std::string author, bool caseSens = true, std::size_t maxRes = 100)
            : targetAuthor(std::move(author)), caseSensitive(caseSens), maxResults(maxRes) {
            if (targetAuthor.empty()) {
                throw std::invalid_argument("AuthorHistoryQuery: author name cannot be empty");
            }
            if (maxResults == 0) {
                throw std::invalid_argument("AuthorHistoryQuery: maxResults cannot be 0");
            }
        }

        [[nodiscard]] std::vector<CommitRecord> apply(const std::vector<CommitRecord>& commits) const override {
            std::vector<CommitRecord> result;
            std::string targetCmp = caseSensitive ? targetAuthor : toLowerCopy(targetAuthor);

            for (const auto& c : commits) {
                std::string nameCmp = caseSensitive ? c.authorName : toLowerCopy(c.authorName);
                std::string emailCmp = caseSensitive ? c.authorEmail : toLowerCopy(c.authorEmail);

                if (nameCmp == targetCmp || emailCmp == targetCmp) {
                    result.push_back(c);
                    if (result.size() >= maxResults) break;
                }
            }
            return result;
        }
    };

    class MessageKeywordQuery : public HistoryQuery {
    private:
        std::string keyword;
        bool matchCase;
        std::size_t minMessageLength;
    public:
        MessageKeywordQuery(std::string kw, bool matchC = false, std::size_t minLen = 0)
            : keyword(std::move(kw)), matchCase(matchC), minMessageLength(minLen) {
            if (keyword.empty()) {
                throw std::invalid_argument("MessageKeywordQuery: search keyword cannot be empty");
            }
        }

        [[nodiscard]] std::vector<CommitRecord> apply(const std::vector<CommitRecord>& commits) const override {
            std::vector<CommitRecord> result;
            std::string kwCmp = matchCase ? keyword : toLowerCopy(keyword);

            for (const auto& c : commits) {
                if (c.message.length() >= minMessageLength) {
                    std::string msgCmp = matchCase ? c.message : toLowerCopy(c.message);
                    if (msgCmp.find(kwCmp) != std::string::npos) {
                        result.push_back(c);
                    }
                }
            }
            return result;
        }
    };


    class MergeCommitQuery : public HistoryQuery {
    private:
        bool onlyMerges;
        std::size_t minParents;
    public:
        explicit MergeCommitQuery(bool mergesOnly = true, std::size_t minP = 2)
            : onlyMerges(mergesOnly), minParents(minP) {
            if (mergesOnly && minParents < 2) {
                throw std::invalid_argument("MergeCommitQuery: minParents must be at least 2 when onlyMerges is true");
            }
        }

        [[nodiscard]] std::vector<CommitRecord> apply(const std::vector<CommitRecord>& commits) const override {
            std::vector<CommitRecord> result;
            for (const auto& c : commits) {
                if (onlyMerges) {
                    if (c.parentOids.size() >= minParents) {
                        result.push_back(c);
                    }
                }
                else {
                    if (c.parentOids.size() <= 1) {
                        result.push_back(c);
                    }
                }
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
        bool allowOverflow;
    public:
        explicit LastNSelection(std::size_t n, bool overflow = true) : count(n), allowOverflow(overflow) {
            if (n == 0 && !overflow) {
                throw std::invalid_argument("LastNSelection: count cannot be 0 when overflow is disabled");
            }
        }

        [[nodiscard]] std::vector<CommitRecord> select(const std::vector<CommitRecord>& commits) const override {
            if (commits.size() <= count) {
                if (!allowOverflow && commits.size() < count) {
                    throw std::runtime_error("LastNSelection: requested count exceeds available commits and overflow is disabled");
                }
                return commits;
            }
            return std::vector<CommitRecord>(commits.begin(), commits.begin() + count);
        }
    };

    class ExplicitSelection : public RevisionSelection {
    private:
        std::vector<Oid> targetOids;
        bool strictExistence;
    public:
        explicit ExplicitSelection(std::vector<Oid> oids, bool strict = true)
            : targetOids(std::move(oids)), strictExistence(strict) {
            if (targetOids.empty()) {
                throw std::invalid_argument("ExplicitSelection: target OIDs list cannot be empty");
            }
        }

        [[nodiscard]] std::vector<CommitRecord> select(const std::vector<CommitRecord>& commits) const override {
            std::unordered_set<Oid> requestedSet(targetOids.begin(), targetOids.end());

            if (strictExistence) {
                for (const auto& reqOid : targetOids) {
                    bool found = std::any_of(commits.begin(), commits.end(), [&](const CommitRecord& c) {
                        return c.oid == reqOid;
                        });
                    if (!found) {
                        throw std::invalid_argument("ExplicitSelection: requested OID not found: " + reqOid);
                    }
                }
            }

            std::vector<CommitRecord> result;
            for (const auto& c : commits) {
                if (requestedSet.erase(c.oid) > 0) {
                    result.push_back(c);
                }
            }
            return result;
        }
    };

    class PeriodicSelection : public RevisionSelection {
    private:
        std::size_t step;
        std::size_t offset;
    public:
        explicit PeriodicSelection(std::size_t s, std::size_t off = 0) : step(s), offset(off) {
            if (s == 0) {
                throw std::invalid_argument("PeriodicSelection: step size cannot be 0");
            }
        }

        [[nodiscard]] std::vector<CommitRecord> select(const std::vector<CommitRecord>& commits) const override {
            std::vector<CommitRecord> result;
            if (commits.empty() || offset >= commits.size()) {
                return result;
            }

            for (std::size_t i = offset; i < commits.size(); ) {
                result.push_back(commits[i]);
                // Safe check for size_t overflow before adding step
                if (commits.size() - i <= step) {
                    break;
                }
                i += step;
            }
            return result;
        }
    };

    class AuthorSelection : public RevisionSelection {
    private:
        std::string authorName;
        bool matchEmailToo;
        bool caseSensitive;
    public:
        explicit AuthorSelection(std::string name, bool matchEmail = false, bool caseSens = true)
            : authorName(std::move(name)), matchEmailToo(matchEmail), caseSensitive(caseSens) {
            if (authorName.empty()) {
                throw std::invalid_argument("AuthorSelection: author name cannot be empty");
            }
        }

        [[nodiscard]] std::vector<CommitRecord> select(const std::vector<CommitRecord>& commits) const override {
            std::vector<CommitRecord> result;
            std::string targetCmp = caseSensitive ? authorName : toLowerCopy(authorName);

            for (const auto& c : commits) {
                std::string nameCmp = caseSensitive ? c.authorName : toLowerCopy(c.authorName);
                std::string emailCmp = caseSensitive ? c.authorEmail : toLowerCopy(c.authorEmail);

                if (nameCmp == targetCmp || (matchEmailToo && emailCmp == targetCmp)) {
                    result.push_back(c);
                }
            }
            return result;
        }
    };

    class DateRangeSelection : public RevisionSelection {
    private:
        std::chrono::sys_seconds fromTime;
        std::chrono::sys_seconds toTime;
        bool inclusive;
    public:
        DateRangeSelection(std::chrono::sys_seconds from, std::chrono::sys_seconds to, bool incl = true)
            : fromTime(from), toTime(to), inclusive(incl) {
            if (fromTime > toTime) {
                throw std::invalid_argument("DateRangeSelection: invalid time interval (from > to)");
            }
        }

        [[nodiscard]] std::vector<CommitRecord> select(const std::vector<CommitRecord>& commits) const override {
            std::vector<CommitRecord> result;
            for (const auto& c : commits) {
                if (inclusive) {
                    if (c.authoredAt >= fromTime && c.authoredAt <= toTime) {
                        result.push_back(c);
                    }
                }
                else {
                    if (c.authoredAt > fromTime && c.authoredAt < toTime) {
                        result.push_back(c);
                    }
                }
            }
            return result;
        }
    };


    class LinearHistorySelection : public RevisionSelection {
    private:
        Oid startOid;
        bool firstParentOnly;
    public:
        explicit LinearHistorySelection(Oid start, bool firstParent = true)
            : startOid(std::move(start)), firstParentOnly(firstParent) {
            if (startOid.empty()) {
                throw std::invalid_argument("LinearHistorySelection: start OID cannot be empty");
            }
        }

        [[nodiscard]] std::vector<CommitRecord> select(const std::vector<CommitRecord>& commits) const override {
            std::vector<CommitRecord> result;
            if (commits.empty()) return result;

            std::unordered_map<Oid, const CommitRecord*> commitMap;
            commitMap.reserve(commits.size());
            for (const auto& c : commits) {
                commitMap[c.oid] = &c;
            }

            auto it = commitMap.find(startOid);
            if (it == commitMap.end()) {
                throw std::invalid_argument("LinearHistorySelection: start OID not found: " + startOid);
            }

            const CommitRecord* current = it->second;
            std::unordered_set<Oid> visited;

            while (current != nullptr) {
                if (!visited.insert(current->oid).second) {
                    break;
                }
                result.push_back(*current);
                if (current->parentOids.empty()) {
                    break;
                }
                const Oid& nextOid = current->parentOids.front();
                auto nextIt = commitMap.find(nextOid);
                current = (nextIt != commitMap.end()) ? nextIt->second : nullptr;
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


    struct CodeChurn {
        std::size_t totalAddedLines = 0;
        std::size_t totalDeletedLines = 0;
        std::int64_t netLines = 0;
        std::size_t binaryFilesCount = 0;
        std::size_t modifiedFilesCount = 0;
    };

    [[nodiscard]] inline CodeChurn computeCodeChurn(const std::vector<ChangeRecord>& changes) {
        CodeChurn churn{};
        churn.modifiedFilesCount = changes.size();
        for (const auto& ch : changes) {
            if (ch.isBinary) {
                churn.binaryFilesCount++;
            }
            if (ch.addedLines.has_value()) {
                churn.totalAddedLines += *ch.addedLines;
            }
            if (ch.deletedLines.has_value()) {
                churn.totalDeletedLines += *ch.deletedLines;
            }
        }
        churn.netLines = static_cast<std::int64_t>(churn.totalAddedLines) - static_cast<std::int64_t>(churn.totalDeletedLines);
        return churn;
    }

    [[nodiscard]] inline bool isAncestor(
        const std::vector<CommitRecord>& commits,
        const Oid& ancestorOid,
        const Oid& descendantOid)
    {
        if (ancestorOid.empty()) {
            throw std::invalid_argument("isAncestor: ancestor OID cannot be empty");
        }
        if (descendantOid.empty()) {
            throw std::invalid_argument("isAncestor: descendant OID cannot be empty");
        }
        if (ancestorOid == descendantOid) {
            return true;
        }

        std::unordered_map<Oid, const CommitRecord*> commitMap;
        commitMap.reserve(commits.size());
        for (const auto& c : commits) {
            commitMap[c.oid] = &c;
        }

        if (commitMap.find(descendantOid) == commitMap.end()) {
            throw std::invalid_argument("isAncestor: descendant OID not found in commits: " + descendantOid);
        }

        std::vector<Oid> queue = { descendantOid };
        std::unordered_set<Oid> visited = { descendantOid };
        std::size_t head = 0;

        while (head < queue.size()) {
            Oid currOid = queue[head++];
            auto it = commitMap.find(currOid);
            if (it == commitMap.end()) continue;

            for (const auto& p : it->second->parentOids) {
                if (p == ancestorOid) {
                    return true;
                }
                if (visited.insert(p).second) {
                    queue.push_back(p);
                }
            }
        }

        return false;
    }

    [[nodiscard]] inline std::vector<ChangeRecord> filterChangesByStatus(
        const std::vector<ChangeRecord>& changes,
        ChangeStatus targetStatus)
    {
        std::vector<ChangeRecord> result;
        for (const auto& ch : changes) {
            if (ch.status == targetStatus) {
                result.push_back(ch);
            }
        }
        return result;
    }

    [[nodiscard]] inline std::vector<CommitRecord> sortCommitsChronologically(
        std::vector<CommitRecord> commits,
        bool ascending = true)
    {
        std::stable_sort(commits.begin(), commits.end(), [ascending](const CommitRecord& a, const CommitRecord& b) {
            return ascending ? (a.authoredAt < b.authoredAt) : (a.authoredAt > b.authoredAt);
        });
        return commits;
    }

} // namespace xray::history
