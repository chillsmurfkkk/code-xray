#include "history/types.hpp"
#include <iostream>
#include <vector>
#include <stdexcept>
#include <string>

void ensure(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error("Test assertion failed: " + message);
    }
}

std::vector<xray::history::CommitRecord> executeQuery(
    const xray::history::HistoryQuery& query,
    const std::vector<xray::history::CommitRecord>& commits)
{
    return query.apply(commits);
}

std::vector<xray::history::CommitRecord> executeSelection(
    const xray::history::RevisionSelection& selection,
    const std::vector<xray::history::CommitRecord>& commits)
{
    return selection.select(commits);
}

int main() {
    using namespace xray::history;

    std::cout << "  CodeXray: History Module Validation (PR #1 Verification)\n";

    std::vector<CommitRecord> mockCommits = {
        {"oid4", {"oid3"}, "test3", "c@test.com", {}, {}, "Add analysis metrics"},
        {"oid3", {"oid2"}, "test2", "b@test.com", {}, {}, "Fix build issues"},
        {"oid2", {"oid1"}, "test1", "a@test.com", {}, {}, "Add code parser"},
        {"oid1", {},       "test2", "b@test.com", {}, {}, "Initial commit"}
    };

    std::vector<ChangeRecord> mockChanges = {
        {"oid4", "oid3", "src/analysis/api.hpp", "src/analysis/api.hpp", ChangeStatus::modified, 30, 0},
        {"oid3", "oid2", "src/history/api.hpp", "src/history/api.hpp", ChangeStatus::modified, 10, 2},
        {"oid2", "oid1", "src/code/api.hpp", "src/code/api.hpp", ChangeStatus::modified, 20, 5},
        {"oid1", std::nullopt, std::nullopt, "src/app/main.cpp", ChangeStatus::added, 15, 0}
    };

    std::cout << "1. Testing HistoryQuery Polymorphism & Boundaries\n";
    {
        CommitRangeQuery rangeQuery("oid4", "oid2");
        auto rangeResult = executeQuery(rangeQuery, mockCommits);
        ensure(rangeResult.size() == 3, "Range query size mismatch");
        ensure(rangeResult[0].oid == "oid4" && rangeResult[2].oid == "oid2", "Range bounds mismatch");
        std::cout << "  [OK] CommitRangeQuery returned valid range oid4..oid2\n";

        bool caughtRangeError = false;
        try {
            CommitRangeQuery invalidRange("non_existing_oid", "oid2");
            executeQuery(invalidRange, mockCommits);
        }
        catch (const std::invalid_argument&) {
            caughtRangeError = true;
        }
        ensure(caughtRangeError, "CommitRangeQuery failed to report invalid boundary OID");
        std::cout << "  [OK] CommitRangeQuery explicitly reported invalid boundary OID\n";

        bool caughtEmptyCommitsError = false;
        try {
            CommitRangeQuery emptyListQuery("oid4", "oid2");
            executeQuery(emptyListQuery, {});
        }
        catch (const std::invalid_argument&) {
            caughtEmptyCommitsError = true;
        }
        ensure(caughtEmptyCommitsError, "CommitRangeQuery failed to report boundary OID on empty commits list");
        std::cout << "  [OK] CommitRangeQuery reported missing OID on empty commits list\n";

        bool caughtInvertedRangeError = false;
        try {
            CommitRangeQuery invertedRange("oid2", "oid4");
            executeQuery(invertedRange, mockCommits);
        }
        catch (const std::invalid_argument&) {
            caughtInvertedRangeError = true;
        }
        ensure(caughtInvertedRangeError, "CommitRangeQuery failed to report inverted range order");
        std::cout << "  [OK] CommitRangeQuery explicitly reported inverted range order\n";

        FileHistoryQuery fileQuery("src/history/api.hpp", mockChanges);
        auto fileResult = executeQuery(fileQuery, mockCommits);
        ensure(fileResult.size() == 1 && fileResult[0].oid == "oid3", "File query result mismatch");
        std::cout << "  [OK] FileHistoryQuery correctly filtered commits touching history/api.hpp\n";
    }

    std::cout << "\n2. Testing RevisionSelection Deduplication & Chain Order\n";
    {
        LastNSelection last2(2);
        auto last2Result = executeSelection(last2, mockCommits);
        ensure(last2Result.size() == 2 && last2Result[0].oid == "oid4", "LastN selection mismatch");
        std::cout << "  [OK] LastNSelection picked 2 newest commits\n";

        ExplicitSelection explicitSel(std::vector<Oid>{"oid1", "oid3", "oid1"});
        auto explicitResult = executeSelection(explicitSel, mockCommits);
        ensure(explicitResult.size() == 2, "ExplicitSelection deduplication failed");
        ensure(explicitResult[0].oid == "oid3" && explicitResult[1].oid == "oid1", "ExplicitSelection failed to preserve chain order");
        std::cout << "  [OK] ExplicitSelection deduplicated OIDs and preserved repository chain order\n";

        bool caughtExplicitError = false;
        try {
            ExplicitSelection badExplicit(std::vector<Oid>{"non_existing_oid"});
            executeSelection(badExplicit, mockCommits);
        }
        catch (const std::invalid_argument&) {
            caughtExplicitError = true;
        }
        ensure(caughtExplicitError, "ExplicitSelection failed to report missing OID");
        std::cout << "  [OK] ExplicitSelection reported missing OID\n";

        PeriodicSelection periodic(2);
        auto periodicResult = executeSelection(periodic, mockCommits);
        ensure(periodicResult.size() == 2 && periodicResult[0].oid == "oid4" && periodicResult[1].oid == "oid2", "Periodic selection mismatch");
        std::cout << "  [OK] PeriodicSelection with step=2 picked {oid4, oid2}\n";

        bool caughtPeriodicError = false;
        try {
            PeriodicSelection badPeriodic(0);
        }
        catch (const std::invalid_argument&) {
            caughtPeriodicError = true;
        }
        ensure(caughtPeriodicError, "PeriodicSelection failed to reject step=0");
        std::cout << "  [OK] PeriodicSelection rejected step=0\n";
    }

    std::cout << "\n3. Testing Generic groupRecords\n";
    {
        auto groupedByAuthor = groupRecords(mockCommits, [](const CommitRecord& c) {
            return c.authorName;
            });
        ensure(groupedByAuthor.size() == 3, "Grouping by author failed");
        std::cout << "  [OK] groupRecords grouped commits by Author into 3 groups\n";

        auto groupedByStatus = groupRecords(mockChanges, [](const ChangeRecord& ch) {
            return ch.status;
            });
        ensure(groupedByStatus.size() == 2, "Grouping by status failed");
        std::cout << "  [OK] groupRecords grouped changes by Status into 2 groups\n";
    }

    std::cout << "  PR #1 VERIFICATION PASSED (OOP BASE & REVISION CORE)\n";

    return 0;
}