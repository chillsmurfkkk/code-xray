#include "history/types.hpp"
#include <iostream>
#include <vector>
#include <cassert>
#include <stdexcept>

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

    std::cout << "  CodeXray: History Module Validation (LR #1 Requirements)\n";

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

    std::cout << "1. Testing HistoryQuery via Polymorphic Interface (HistoryQuery&)\n";
    {
        CommitRangeQuery rangeQuery("oid4", "oid2");
        auto rangeResult = executeQuery(rangeQuery, mockCommits);
        assert(rangeResult.size() == 3);
        assert(rangeResult[0].oid == "oid4" && rangeResult[2].oid == "oid2");
        std::cout << "  [OK] CommitRangeQuery returned valid range oid4..oid2\n";

        CommitRangeQuery invalidRange("non_existing_oid", "oid2");
        auto invalidResult = executeQuery(invalidRange, mockCommits);
        assert(invalidResult.empty());
        std::cout << "  [OK] CommitRangeQuery correctly rejected invalid start boundary\n";

        FileHistoryQuery fileQuery("src/history/api.hpp", mockChanges);
        auto fileResult = executeQuery(fileQuery, mockCommits);
        assert(fileResult.size() == 1 && fileResult[0].oid == "oid3");
        std::cout << "  [OK] FileHistoryQuery correctly filtered commits touching history/api.hpp\n";
    }

    std::cout << "\n2. Testing RevisionSelection Strategies (RevisionSelection&)\n";
    {
        LastNSelection last2(2);
        auto last2Result = executeSelection(last2, mockCommits);
        assert(last2Result.size() == 2 && last2Result[0].oid == "oid4");
        std::cout << "  [OK] LastNSelection picked 2 newest commits\n";

        ExplicitSelection explicitSel(std::vector<Oid>{"oid3", "oid1"});
        auto explicitResult = executeSelection(explicitSel, mockCommits);
        assert(explicitResult.size() == 2 && explicitResult[0].oid == "oid3" && explicitResult[1].oid == "oid1");
        std::cout << "  [OK] ExplicitSelection picked exact OIDs in requested order\n";

        bool caughtExplicitError = false;
        try {
            ExplicitSelection badExplicit(std::vector<Oid>{"non_existing_oid"});
            executeSelection(badExplicit, mockCommits);
        }
        catch (const std::invalid_argument&) {
            caughtExplicitError = true;
        }
        assert(caughtExplicitError);
        std::cout << "  [OK] ExplicitSelection threw exception on non-existing OID\n";

        PeriodicSelection periodic(2);
        auto periodicResult = executeSelection(periodic, mockCommits);
        assert(periodicResult.size() == 2 && periodicResult[0].oid == "oid4" && periodicResult[1].oid == "oid2");
        std::cout << "  [OK] PeriodicSelection with step=2 picked {oid4, oid2}\n";

        bool caughtPeriodicError = false;
        try {
            PeriodicSelection badPeriodic(0);
        }
        catch (const std::invalid_argument&) {
            caughtPeriodicError = true;
        }
        assert(caughtPeriodicError);
        std::cout << "  [OK] PeriodicSelection threw exception on step=0\n";
    }

    std::cout << "\n3. Testing Generic groupRecords\n";
    {
        auto groupedByAuthor = groupRecords(mockCommits, [](const CommitRecord& c) {
            return c.authorName;
            });
        assert(groupedByAuthor.size() == 3); // test3, test2, test1
        std::cout << "  [OK] groupRecords grouped commits by Author into 3 groups\n";

        auto groupedByStatus = groupRecords(mockChanges, [](const ChangeRecord& ch) {
            return ch.status;
            });
        assert(groupedByStatus.size() == 2); // modified, added
        std::cout << "  [OK] groupRecords grouped changes by Status into 2 groups\n";
    }

    std::cout << "  ALL LR #1 REQUIREMENTS SUCCESSFULLY VERIFIED!\n";

    return 0;
}