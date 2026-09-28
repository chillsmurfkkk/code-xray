#include "history/types.hpp"
#include <iostream>
#include <memory>
#include <vector>
#include <cassert>

void printCommitList(const std::string& label, const std::vector<xray::history::CommitRecord>& commits) {
    std::cout << "  [" << label << "] Total: " << commits.size() << " commit(s) -> { ";
    for (size_t i = 0; i < commits.size(); ++i) {
        std::cout << commits[i].oid << (i + 1 < commits.size() ? ", " : "");
    }
    std::cout << " }\n";
}

int main() {
    using namespace xray::history;

    std::vector<CommitRecord> mockCommits = {
        {"oid4", {"oid3"}, "test3", "b@test.com", {}, {}, "Add analysis metrics"},
        {"oid3", {"oid2"}, "test2", "c@test.com", {}, {}, "Fix LNK2019 build issues"},
        {"oid2", {"oid1"}, "test1", "a@test.com", {}, {}, "Add code parser"},
        {"oid1", {},       "test2", "c@test.com", {}, {}, "Initial commit"}
    };

    std::vector<ChangeRecord> mockChanges = {
        {"oid4", "oid3", "src/analysis/api.hpp", "src/analysis/api.hpp", ChangeStatus::modified, 30, 0},
        {"oid3", "oid2", "src/history/api.hpp", "src/history/api.hpp", ChangeStatus::modified, 10, 2},
        {"oid2", "oid1", "src/code/api.hpp", "src/code/api.hpp", ChangeStatus::modified, 20, 5},
        {"oid1", std::nullopt, std::nullopt, "src/app/main.cpp", ChangeStatus::added, 15, 0}
    };

    printCommitList("Input Mock Commits (Newest -> Oldest)", mockCommits);
    std::cout << "1. Testing HistoryQuery Polymorphic Hierarchy\n";

    {
        CommitRangeQuery rangeQuery("oid4", "oid2");
        auto results = rangeQuery.apply(mockCommits);
        printCommitList("CommitRangeQuery (oid4..oid2)", results);

        assert(results.size() == 3);
        assert(results[0].oid == "oid4");
        assert(results[1].oid == "oid3");
        assert(results[2].oid == "oid2");
        std::cout << "   -> OK: Range query returned exact sequence {oid4, oid3, oid2}.\n";

        CommitRangeQuery unknownRange("unknown_1", "unknown_2");
        auto emptyResults = unknownRange.apply(mockCommits);
        printCommitList("CommitRangeQuery (unknown bounds)", emptyResults);
        assert(emptyResults.empty());
        std::cout << "   -> OK: Unknown range handled correctly (0 commits returned).\n";
    }

    {
        FileHistoryQuery fileQuery("src/history/api.hpp", mockChanges);
        auto results = fileQuery.apply(mockCommits);
        printCommitList("FileHistoryQuery ('src/history/api.hpp')", results);

        assert(results.size() == 1);
        assert(results[0].oid == "oid3");
        std::cout << "   -> OK: File history matched commit oid3.\n";
    }

    std::cout << "2. Testing RevisionSelection Edge Cases & OID Selection\n";

    {
        LastNSelection last2(2);
        auto sel1 = last2.select(mockCommits);
        printCommitList("LastNSelection (N=2)", sel1);
        assert(sel1.size() == 2 && sel1[0].oid == "oid4" && sel1[1].oid == "oid3");
        std::cout << "   -> OK: LastNSelection picked 2 newest commits.\n";

        ExplicitSelection explicitSel(std::vector<Oid>{"oid3", "non_existing_oid"});
        auto sel2 = explicitSel.select(mockCommits);
        printCommitList("ExplicitSelection (oid3, non_existing_oid)", sel2);
        assert(sel2.size() == 1 && sel2[0].oid == "oid3");
        std::cout << "   -> OK: ExplicitSelection filtered out non-existing OID.\n";

        PeriodicSelection zeroStep(0);
        auto sel3 = zeroStep.select(mockCommits);
        printCommitList("PeriodicSelection (step=0 fallback)", sel3);
        assert(sel3.size() == 4);
        std::cout << "   -> OK: Zero step fallback handled safely (treated as step=1).\n";

        PeriodicSelection step2(2);
        auto sel4 = step2.select(mockCommits);
        printCommitList("PeriodicSelection (step=2)", sel4);
        assert(sel4.size() == 2 && sel4[0].oid == "oid4" && sel4[1].oid == "oid2");
        std::cout << "   -> OK: Periodic selection picked {oid4, oid2}.\n";
    }

    std::cout << "3. Testing groupRecords Template (Commits & Changes)\n";

    {
        auto groupedByAuthor = groupRecords(mockCommits, [](const CommitRecord& c) {
            return c.authorName;
            });

        std::cout << "  [groupRecords by Author] Created " << groupedByAuthor.size() << " group(s):\n";
        for (const auto& group : groupedByAuthor) {
            std::cout << "    - Author '" << group[0].authorName << "' (" << group.size() << " commits): { ";
            for (size_t i = 0; i < group.size(); ++i) {
                std::cout << group[i].oid << (i + 1 < group.size() ? ", " : "");
            }
            std::cout << " }\n";
        }

        assert(groupedByAuthor.size() == 3);

        auto it = std::find_if(groupedByAuthor.begin(), groupedByAuthor.end(), [](const std::vector<CommitRecord>& g) {
            return !g.empty() && g[0].authorName == "test2";
            });
        assert(it != groupedByAuthor.end());
        assert(it->size() == 2);
        assert((*it)[0].oid == "oid3");
        assert((*it)[1].oid == "oid1");
        std::cout << "   -> OK: Author 'test2' commits (oid3, oid1) correctly merged into 1 group.\n";

        auto groupedByStatus = groupRecords(mockChanges, [](const ChangeRecord& ch) {
            return ch.status;
            });
        std::cout << "  [groupRecords by ChangeStatus] Created " << groupedByStatus.size() << " status group(s).\n";
        assert(groupedByStatus.size() == 2);
        std::cout << "   -> OK: Changes correctly grouped into 2 status categories (added, modified).\n";
    }

    std::cout << "  SUCCESS: All history module tests & assertions passed! \n";

    return 0;
}