#include "history/types.hpp"
#include <iostream>
#include <memory>
#include <vector>
#include <cassert>

int main() {
    using namespace xray::history;

    std::vector<CommitRecord> mockCommits = {
        {"oid1", {}, "test2", "c@test.com", {}, {}, "Initial commit"},
        {"oid2", {"oid1"}, "test1", "a@test.com", {}, {}, "Add code parser"},
        {"oid3", {"oid2"}, "test2", "c@test.com", {}, {}, "Fix LNK2019 build issues"},
        {"oid4", {"oid3"}, "test3", "b@test.com", {}, {}, "Add analysis metrics"}
    };

    std::vector<ChangeRecord> mockChanges = {
        {"oid1", std::nullopt, std::nullopt, "src/app/main.cpp", ChangeStatus::added, 15, 0},
        {"oid2", "oid1", "src/code/api.hpp", "src/code/api.hpp", ChangeStatus::modified, 20, 5},
        {"oid3", "oid2", "src/history/api.hpp", "src/history/api.hpp", ChangeStatus::modified, 10, 2},
        {"oid4", "oid3", "src/analysis/api.hpp", "src/analysis/api.hpp", ChangeStatus::modified, 30, 0}
    };

    std::unique_ptr<HistoryQuery> rangeQuery = std::make_unique<CommitRangeQuery>("oid2", "oid4");
    auto rangeResults = rangeQuery->apply(mockCommits);
    assert(rangeResults.size() == 3 && "CommitRangeQuery failed");

    std::unique_ptr<HistoryQuery> fileQuery = std::make_unique<FileHistoryQuery>("src/history/api.hpp", mockChanges);
    auto fileResults = fileQuery->apply(mockCommits);
    assert(fileResults.size() == 1 && "FileHistoryQuery failed");

    LastNSelection last2(2);
    assert(last2.select(mockCommits).size() == 2 && "LastNSelection failed");

    ExplicitSelection explicitSel(std::vector<Oid>{"oid1", "oid3"});
    assert(explicitSel.select(mockCommits).size() == 2 && "ExplicitSelection failed");

    PeriodicSelection periodicSel(2);
    assert(periodicSel.select(mockCommits).size() == 2 && "PeriodicSelection failed");

    auto groupedByAuthor = groupRecords(mockCommits, [](const CommitRecord& c) {
        return c.authorName;
        });
    assert(groupedByAuthor.size() == 3 && "groupRecords for commits failed: expected 3 unique authors");
    assert(groupedByAuthor[0].size() == 2 && "Author 'test2' should have 2 commits");

    auto groupedByStatus = groupRecords(mockChanges, [](const ChangeRecord& ch) {
        return ch.status;
        });
    assert(groupedByStatus.size() == 2 && "groupRecords for changes failed: expected 2 status groups (added, modified)");

    std::cout << "All history module assertion tests passed successfully!\n";
    return 0;
}