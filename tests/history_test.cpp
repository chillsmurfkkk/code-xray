#include "history/types.hpp"
#include <iostream>
#include <memory>
#include <vector>

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

    std::cout << "--- Testing HistoryQuery Hierarchy ---\n";

    std::unique_ptr<HistoryQuery> rangeQuery = std::make_unique<CommitRangeQuery>("oid2", "oid4");
    auto rangeResults = rangeQuery->apply(mockCommits);
    std::cout << "[CommitRangeQuery] Matched commits count (oid2..oid4): " << rangeResults.size() << "\n";

    std::unique_ptr<HistoryQuery> fileQuery = std::make_unique<FileHistoryQuery>("src/history/api.hpp", mockChanges);
    auto fileResults = fileQuery->apply(mockCommits);
    std::cout << "[FileHistoryQuery] Matched commits touching 'src/history/api.hpp': " << fileResults.size() << "\n\n";

    std::cout << "--- Testing RevisionSelection Strategies ---\n";

    std::vector<std::unique_ptr<RevisionSelection>> strategies;
    strategies.push_back(std::make_unique<LastNSelection>(2));
    strategies.push_back(std::make_unique<ExplicitSelection>(std::vector<Oid>{"oid1", "oid3"}));
    strategies.push_back(std::make_unique<PeriodicSelection>(2));

    for (std::size_t i = 0; i < strategies.size(); ++i) {
        auto selected = strategies[i]->select(mockCommits);
        std::cout << "Strategy #" << (i + 1) << " selected commits count: " << selected.size() << "\n";
    }
    std::cout << "\n";

    // 4. Тест шаблонного groupRecords
    std::cout << "--- Testing groupRecords Template ---\n";

    auto groupedByAuthor = groupRecords(mockCommits, [](const CommitRecord& c) {
        return c.authorName;
        });
    std::cout << "Grouped commit blocks by author count: " << groupedByAuthor.size() << "\n";

    std::cout << "\n=== All tests completed successfully! ===\n";

    return 0;
}