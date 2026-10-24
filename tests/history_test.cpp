#include "history/types.hpp"
#include <iostream>
#include <vector>
#include <stdexcept>
#include <string>
#include <chrono>
#include <climits>

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
    using namespace std::chrono;

    std::cout << "  CodeXray: History Module Validation (PR #1 Verification)\n";

    auto t1 = sys_seconds{ seconds(1000) };
    auto t2 = sys_seconds{ seconds(2000) };
    auto t3 = sys_seconds{ seconds(3000) };
    auto t4 = sys_seconds{ seconds(4000) };

    std::vector<CommitRecord> mockCommits = {
        {"oid4", {"oid3"}, "TestAuthor", "c@test.com", t4, t4, "Add Analysis Metrics"},
        {"oid3", {"oid2"}, "test2", "b@test.com", t3, t3, "Fix build issues"},
        {"oid2", {"oid1"}, "test1", "a@test.com", t2, t2, "Add code parser"},
        {"oid1", {},        "test2", "b@test.com", t1, t1, "Initial commit"}
    };

    std::vector<ChangeRecord> mockChanges = {
        {"oid4", "oid3", "src/analysis/api.hpp", "src/analysis/api.hpp", ChangeStatus::modified, 30, 0},
        {"oid3", "oid2", "src/history/api.hpp", "src/history/api.hpp", ChangeStatus::modified, 10, 2},
        {"oid2", {"oid1"}, "src/code/api.hpp", "src/code/api.hpp", ChangeStatus::modified, 20, 5},
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

        FileHistoryQuery fileQueryStrict("history/api.hpp", mockChanges, true);
        auto strictRes = executeQuery(fileQueryStrict, mockCommits);
        ensure(strictRes.empty(), "FileHistoryQuery strict match should not match partial path");

        FileHistoryQuery fileQueryStrictExact("src/history/api.hpp", mockChanges, true);
        auto strictExactRes = executeQuery(fileQueryStrictExact, mockCommits);
        ensure(strictExactRes.size() == 1 && strictExactRes[0].oid == "oid3", "FileHistoryQuery exact strict match failed");

        FileHistoryQuery fileQueryLoose("history/api.hpp", mockChanges, false);
        auto looseRes = executeQuery(fileQueryLoose, mockCommits);
        ensure(looseRes.size() == 1 && looseRes[0].oid == "oid3", "FileHistoryQuery loose match failed");
        std::cout << "  [OK] FileHistoryQuery correctly verified strictMatch options (true/false)\n";

        AuthorHistoryQuery authorQuerySensitive("testauthor", true);
        auto authorSensRes = executeQuery(authorQuerySensitive, mockCommits);
        ensure(authorSensRes.empty(), "AuthorHistoryQuery case sensitive should fail for different case");

        AuthorHistoryQuery authorQueryInsensitive("testauthor", false);
        auto authorInsensRes = executeQuery(authorQueryInsensitive, mockCommits);
        ensure(authorInsensRes.size() == 1 && authorInsensRes[0].oid == "oid4", "AuthorHistoryQuery case insensitive failed");
        std::cout << "  [OK] AuthorHistoryQuery correctly verified caseSensitive options\n";

        MessageKeywordQuery keywordSens("ANALYSIS", true);
        auto kwSensRes = executeQuery(keywordSens, mockCommits);
        ensure(kwSensRes.empty(), "MessageKeywordQuery case-sensitive match failed");

        MessageKeywordQuery keywordInsens("ANALYSIS", false);
        auto kwInsensRes = executeQuery(keywordInsens, mockCommits);
        ensure(kwInsensRes.size() == 1 && kwInsensRes[0].oid == "oid4", "MessageKeywordQuery case-insensitive match failed");
        std::cout << "  [OK] MessageKeywordQuery correctly verified case-insensitive search\n";
    }

    std::cout << "\n2. Testing RevisionSelection Deduplication & Chain Order\n";
    {
        LastNSelection last2(2);
        auto last2Result = executeSelection(last2, mockCommits);
        ensure(last2Result.size() == 2 && last2Result[0].oid == "oid4", "LastN selection mismatch");
        std::cout << "  [OK] LastNSelection picked 2 newest commits\n";

        bool caughtOverflowError = false;
        try {
            LastNSelection strictN(10, false);
            executeSelection(strictN, mockCommits);
        }
        catch (const std::runtime_error&) {
            caughtOverflowError = true;
        }
        ensure(caughtOverflowError, "LastNSelection failed to throw when allowOverflow=false and count exceeds size");

        LastNSelection looseN(10, true);
        auto looseNResult = executeSelection(looseN, mockCommits);
        ensure(looseNResult.size() == 4, "LastNSelection allowOverflow=true should return all commits when count exceeds size");
        std::cout << "  [OK] LastNSelection correctly enforced allowOverflow (true/false)\n";

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

        PeriodicSelection overflowPeriodic(SIZE_MAX, 1);
        auto overflowRes = executeSelection(overflowPeriodic, mockCommits);
        ensure(overflowRes.size() == 1 && overflowRes[0].oid == "oid3", "PeriodicSelection overflow protection failed");

        std::vector<CommitRecord> twoCommits = { mockCommits[0], mockCommits[1] };
        PeriodicSelection overflowTwo(SIZE_MAX, 1);
        auto twoRes = executeSelection(overflowTwo, twoCommits);
        ensure(twoRes.size() == 1 && twoRes[0].oid == mockCommits[1].oid, "PeriodicSelection(SIZE_MAX, 1) on 2 commits must return only second commit");
        std::cout << "  [OK] PeriodicSelection successfully prevented size_t overflow\n";

        bool caughtPeriodicError = false;
        try {
            PeriodicSelection badPeriodic(0);
        }
        catch (const std::invalid_argument&) {
            caughtPeriodicError = true;
        }
        ensure(caughtPeriodicError, "PeriodicSelection failed to reject step=0");
        std::cout << "  [OK] PeriodicSelection rejected step=0\n";

        AuthorSelection authorSel("test1");
        auto authorSelResult = executeSelection(authorSel, mockCommits);
        ensure(authorSelResult.size() == 1 && authorSelResult[0].oid == "oid2", "AuthorSelection mismatch");

        AuthorSelection authorSensSel("TEST1", false, true);
        auto sensResult = executeSelection(authorSensSel, mockCommits);
        ensure(sensResult.empty(), "AuthorSelection case-sensitive should fail for mismatched case");

        AuthorSelection authorInsensSel("TEST1", false, false);
        auto insensResult = executeSelection(authorInsensSel, mockCommits);
        ensure(insensResult.size() == 1 && insensResult[0].oid == "oid2", "AuthorSelection case-insensitive should match TEST1 to test1");

        AuthorSelection authorEmailSel("a@test.com", true, true);
        auto emailResult = executeSelection(authorEmailSel, mockCommits);
        ensure(emailResult.size() == 1 && emailResult[0].oid == "oid2", "AuthorSelection matchEmailToo=true failed");
        std::cout << "  [OK] AuthorSelection picked correct commits and verified caseSensitive/matchEmailToo\n";

        DateRangeSelection dateRangeSel(t2, t3, true);
        auto dateRangeResult = executeSelection(dateRangeSel, mockCommits);
        ensure(dateRangeResult.size() == 2, "DateRangeSelection size mismatch");
        ensure(dateRangeResult[0].oid == "oid3" && dateRangeResult[1].oid == "oid2", "DateRangeSelection boundaries or order mismatch");
        std::cout << "  [OK] DateRangeSelection correctly included both boundaries (t2 and t3) and dropped out-of-range commits\n";

        bool caughtDateOrderError = false;
        try {
            DateRangeSelection invalidDateRange(t3, t2);
            executeSelection(invalidDateRange, mockCommits);
        }
        catch (const std::invalid_argument&) {
            caughtDateOrderError = true;
        }
        ensure(caughtDateOrderError, "DateRangeSelection failed to reject from > to interval");
        std::cout << "  [OK] DateRangeSelection explicitly rejected invalid interval (from > to)\n";

        CommitRecord specialCommit{ "oid_special", {}, "spec", "s@test.com", sys_seconds{seconds(5000)}, sys_seconds{seconds(1500)}, "Special date test" };
        std::vector<CommitRecord> mixedCommits = { specialCommit };
        DateRangeSelection checkAuthoredAtSel(sys_seconds{ seconds(4500) }, sys_seconds{ seconds(5500) }, true);
        auto authoredAtResult = executeSelection(checkAuthoredAtSel, mixedCommits);
        ensure(authoredAtResult.size() == 1, "DateRangeSelection must filter based on authoredAt, not committedAt");
        std::cout << "  [OK] DateRangeSelection correctly relies on authoredAt timestamp\n";
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