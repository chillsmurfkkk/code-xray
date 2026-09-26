#pragma once

#include "history/types.hpp"
#include "common/job.hpp"
#include "common/result.hpp"

#include <chrono>
#include <iostream>

namespace xray::history {
    inline Result<HistoryResult> query(const RepositorySpec& spec, const HistoryRequest& request, const JobContext& ctx) {
        if (spec.path.empty()) {
            return Error{ .message = "Repository path is empty" };
        }

        HistoryResult result;

        // test data
        CommitRecord commit;
        commit.oid = request.startOid.empty() ? "0000000000000000000000000000000000000000" : request.startOid;
        commit.authorName = "C";
        commit.authorEmail = "testemail.com";
        commit.message = "Lab 1 test commit";
        commit.authoredAt = std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now());
        commit.committedAt = commit.authoredAt;

        result.commits.push_back(commit);

        // test change
        ChangeRecord change;
        change.commitOid = commit.oid;
        change.newPath = request.relativePath.value_or("src/main.cpp");
        change.status = ChangeStatus::added;
        change.addedLines = 10;
        change.deletedLines = 0;

        result.changes.push_back(change);
        result.hasMore = false;

        return result;
    }

    inline Result<code::SourceSnapshot> loadSnapshot(const RepositorySpec& spec, const Oid& oid,
        const code::FileSelection& selection, const JobContext& ctx) {
        if (spec.path.empty() || oid.empty()) {
            return Error{ .message = "Invalid arguments for a snapshot" };
        }

        code::SourceSnapshot snapshot;
        return snapshot;
    }

    template<typename Predicate>
    std::vector<std::vector<CommitRecord>> groupCommits(const std::vector<CommitRecord>& commits, Predicate pred) {
        std::vector<std::vector<CommitRecord>> groups;
        if (commits.empty()) return groups;

        std::vector<CommitRecord> currentGroup;
        for (const auto& c : commits) {
            if (!currentGroup.empty() && !pred(currentGroup.back(), c)) {
                groups.push_back(currentGroup);
                currentGroup.clear();
            }
            currentGroup.push_back(c);
        }
        if (!currentGroup.empty()) groups.push_back(currentGroup);
        return groups;
    }

} // namespace xray::history