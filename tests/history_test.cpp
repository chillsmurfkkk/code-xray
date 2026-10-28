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

    std::cout << "\n4. Testing MergeCommitQuery\n";
    {
        // Add a merge commit (2 parents) to mockCommits
        CommitRecord mergeCommit{ "oid_merge", {"oid4", "oid3"}, "merger", "m@test.com", t4, t4, "Merge branch X" };
        std::vector<CommitRecord> mixedCommits = { mergeCommit, mockCommits[0], mockCommits[1], mockCommits[2], mockCommits[3] };

        MergeCommitQuery mergeOnly(true);
        auto mergeResult = executeQuery(mergeOnly, mixedCommits);
        ensure(mergeResult.size() == 1 && mergeResult[0].oid == "oid_merge", "MergeCommitQuery(true) should return only merge commits");
        std::cout << "  [OK] MergeCommitQuery(true) returned only merge commits\n";

        MergeCommitQuery nonMergeOnly(false);
        auto nonMergeResult = executeQuery(nonMergeOnly, mixedCommits);
        ensure(nonMergeResult.size() == 4, "MergeCommitQuery(false) should return only non-merge commits");
        std::cout << "  [OK] MergeCommitQuery(false) returned only non-merge commits\n";

        bool caughtMergeError = false;
        try {
            MergeCommitQuery bad(true, 1);
        }
        catch (const std::invalid_argument&) {
            caughtMergeError = true;
        }
        ensure(caughtMergeError, "MergeCommitQuery(true, 1) must throw - minParents < 2 while onlyMerges=true");
        std::cout << "  [OK] MergeCommitQuery(true, 1) correctly threw invalid_argument\n";
    }

    std::cout << "\n5. Testing LinearHistorySelection\n";
    {
        // Full chain: all 4 commits present, no truncation.
        LinearHistorySelection linSel("oid4");
        auto linResult = executeSelection(linSel, mockCommits);
        ensure(linResult.size() == 4, "LinearHistorySelection from oid4 should follow chain to oid1");
        ensure(linResult[0].oid == "oid4", "LinearHistorySelection first commit must be oid4");
        ensure(linResult[3].oid == "oid1", "LinearHistorySelection last commit must be oid1");
        std::cout << "  [OK] LinearHistorySelection traversed full chain oid4->oid3->oid2->oid1\n";

        // Partial chain starting from oid2.
        LinearHistorySelection linSel2("oid2");
        auto linResult2 = executeSelection(linSel2, mockCommits);
        ensure(linResult2.size() == 2, "LinearHistorySelection from oid2 should return oid2 and oid1");
        ensure(linResult2[0].oid == "oid2" && linResult2[1].oid == "oid1", "LinearHistorySelection from oid2 chain mismatch");
        std::cout << "  [OK] LinearHistorySelection(oid2) traversed partial chain oid2->oid1\n";

        // Empty OID rejected at construction time.
        bool caughtEmptyOid = false;
        try {
            LinearHistorySelection badSel("");
        }
        catch (const std::invalid_argument&) {
            caughtEmptyOid = true;
        }
        ensure(caughtEmptyOid, "LinearHistorySelection must throw for empty start OID");
        std::cout << "  [OK] LinearHistorySelection correctly rejected empty start OID\n";

        // Start OID missing from the provided commits.
        bool caughtNotFound = false;
        try {
            LinearHistorySelection notFoundSel("nonexistent_oid");
            executeSelection(notFoundSel, mockCommits);
        }
        catch (const std::invalid_argument&) {
            caughtNotFound = true;
        }
        ensure(caughtNotFound, "LinearHistorySelection must throw when start OID not found");
        std::cout << "  [OK] LinearHistorySelection correctly threw when start OID not found\n";

        // Empty commit list must throw (cannot verify start OID exists).
        bool caughtEmptyList = false;
        try {
            LinearHistorySelection emptySel("oid4");
            executeSelection(emptySel, {});
        }
        catch (const std::invalid_argument&) {
            caughtEmptyList = true;
        }
        ensure(caughtEmptyList, "LinearHistorySelection must throw when commit list is empty");
        std::cout << "  [OK] LinearHistorySelection correctly threw on empty commit list\n";

        // Truncated graph: oid3 references oid2 as parent but oid2 is absent.
        std::vector<CommitRecord> truncated = { mockCommits[0], mockCommits[1] }; // oid4, oid3; oid2 missing
        bool caughtTruncated = false;
        try {
            LinearHistorySelection truncSel("oid4");
            executeSelection(truncSel, truncated);
        }
        catch (const std::invalid_argument&) {
            caughtTruncated = true;
        }
        ensure(caughtTruncated, "LinearHistorySelection must throw when parent OID is missing (truncated graph)");
        std::cout << "  [OK] LinearHistorySelection correctly threw on truncated graph (missing parent)\n";
    }

    std::cout << "\n6. Testing computeCodeChurn\n";
    {
        // mockChanges: all records have known line counts -> hasUnknownLineCounts must be false.
        CodeChurn churn = computeCodeChurn(mockChanges);
        // modified(+30,-0), modified(+10,-2), modified(+20,-5), added(+15,-0)
        ensure(churn.totalAddedLines == 75, "computeCodeChurn totalAddedLines mismatch");
        ensure(churn.totalDeletedLines == 7, "computeCodeChurn totalDeletedLines mismatch");
        ensure(churn.netLines == 68, "computeCodeChurn netLines mismatch");
        ensure(churn.binaryFilesCount == 0, "computeCodeChurn binaryFilesCount should be 0");
        ensure(churn.modifiedFilesCount == 4, "computeCodeChurn modifiedFilesCount should be 4");
        ensure(!churn.hasUnknownLineCounts, "computeCodeChurn hasUnknownLineCounts should be false when all counts known");
        std::cout << "  [OK] computeCodeChurn correctly aggregated lines and file counts\n";

        // Empty input: all zeros, no unknown counts.
        CodeChurn emptyChurn = computeCodeChurn({});
        ensure(emptyChurn.totalAddedLines == 0 && emptyChurn.netLines == 0, "computeCodeChurn on empty changes should be zero");
        ensure(!emptyChurn.hasUnknownLineCounts, "computeCodeChurn on empty input should not set hasUnknownLineCounts");
        std::cout << "  [OK] computeCodeChurn on empty input returned zero CodeChurn\n";

        // Record with nullopt line counts: hasUnknownLineCounts must be set.
        ChangeRecord unknownRecord{ "oid_u", std::nullopt, std::nullopt, "file.bin", ChangeStatus::modified,
                                    std::nullopt, std::nullopt };
        CodeChurn unknownChurn = computeCodeChurn({ unknownRecord });
        ensure(unknownChurn.totalAddedLines == 0, "computeCodeChurn with nullopt lines should not add to total");
        ensure(unknownChurn.hasUnknownLineCounts, "computeCodeChurn must set hasUnknownLineCounts when addedLines/deletedLines are nullopt");
        std::cout << "  [OK] computeCodeChurn correctly set hasUnknownLineCounts for record with nullopt line stats\n";
    }

    std::cout << "\n7. Testing isAncestor\n";
    {
        // Standard ancestry cases with complete graph.
        ensure(isAncestor(mockCommits, "oid1", "oid4"), "oid1 must be ancestor of oid4");
        std::cout << "  [OK] isAncestor(oid1, oid4) = true\n";

        ensure(!isAncestor(mockCommits, "oid4", "oid1"), "oid4 must NOT be ancestor of oid1");
        std::cout << "  [OK] isAncestor(oid4, oid1) = false\n";

        ensure(isAncestor(mockCommits, "oid2", "oid4"), "oid2 must be ancestor of oid4");
        std::cout << "  [OK] isAncestor(oid2, oid4) = true\n";

        // Reflexive case: valid only when both OIDs are found in commits.
        ensure(isAncestor(mockCommits, "oid3", "oid3"), "same OID must be its own ancestor");
        std::cout << "  [OK] isAncestor(oid3, oid3) = true (reflexive, both OIDs verified)\n";

        // Empty OID rejected before map lookup.
        bool caughtEmptyAncestor = false;
        try {
            (void)isAncestor(mockCommits, "", "oid4");
        }
        catch (const std::invalid_argument&) {
            caughtEmptyAncestor = true;
        }
        ensure(caughtEmptyAncestor, "isAncestor must throw for empty ancestor OID");
        std::cout << "  [OK] isAncestor threw for empty ancestor OID\n";

        // Missing ancestor OID must throw (previously gave false confidence).
        bool caughtMissingAncestor = false;
        try {
            (void)isAncestor(mockCommits, "nonexistent_ancestor", "oid4");
        }
        catch (const std::invalid_argument&) {
            caughtMissingAncestor = true;
        }
        ensure(caughtMissingAncestor, "isAncestor must throw when ancestor OID not found in commits");
        std::cout << "  [OK] isAncestor threw for missing ancestor OID\n";

        // Missing descendant OID must throw.
        bool caughtMissingDescendant = false;
        try {
            (void)isAncestor(mockCommits, "oid1", "nonexistent");
        }
        catch (const std::invalid_argument&) {
            caughtMissingDescendant = true;
        }
        ensure(caughtMissingDescendant, "isAncestor must throw when descendant OID not found");
        std::cout << "  [OK] isAncestor threw for missing descendant OID\n";

        // Truncated graph: intermediate commit missing -> must throw, not silently return false.
        // oid4->oid3 present; oid2 absent; isAncestor(oid1, oid4) would need oid2 to continue.
        std::vector<CommitRecord> truncated2 = { mockCommits[0], mockCommits[1] }; // oid4, oid3
        // Add oid1 so both endpoints exist but oid2 is still missing in the chain.
        truncated2.push_back(mockCommits[3]); // oid1
        bool caughtTruncatedGraph = false;
        try {
            (void)isAncestor(truncated2, "oid1", "oid4");
        }
        catch (const std::invalid_argument&) {
            caughtTruncatedGraph = true;
        }
        ensure(caughtTruncatedGraph, "isAncestor must throw on truncated graph (intermediate commit missing)");
        std::cout << "  [OK] isAncestor threw on truncated graph instead of giving uncertain false\n";
    }

    std::cout << "\n8. Testing filterChangesByStatus\n";
    {
        auto modifiedOnly = filterChangesByStatus(mockChanges, ChangeStatus::modified);
        ensure(modifiedOnly.size() == 3, "filterChangesByStatus(modified) should return 3 records");
        std::cout << "  [OK] filterChangesByStatus returned 3 modified records\n";

        auto addedOnly = filterChangesByStatus(mockChanges, ChangeStatus::added);
        ensure(addedOnly.size() == 1, "filterChangesByStatus(added) should return 1 record");
        ensure(addedOnly[0].commitOid == "oid1", "filterChangesByStatus(added) record should be from oid1");
        std::cout << "  [OK] filterChangesByStatus returned 1 added record\n";

        auto deletedOnly = filterChangesByStatus(mockChanges, ChangeStatus::deleted);
        ensure(deletedOnly.empty(), "filterChangesByStatus(deleted) should return 0 records");
        std::cout << "  [OK] filterChangesByStatus returned 0 deleted records (none in mock data)\n";
    }

    std::cout << "\n9. Testing sortCommitsChronologically\n";
    {
        // mockCommits is ordered newest-first: oid4(t4), oid3(t3), oid2(t2), oid1(t1)
        auto ascending = sortCommitsChronologically(mockCommits, true);
        ensure(ascending[0].oid == "oid1" && ascending[3].oid == "oid4", "sortCommitsChronologically ascending failed");
        std::cout << "  [OK] sortCommitsChronologically(ascending=true): oid1->oid2->oid3->oid4\n";

        auto descending = sortCommitsChronologically(mockCommits, false);
        ensure(descending[0].oid == "oid4" && descending[3].oid == "oid1", "sortCommitsChronologically descending failed");
        std::cout << "  [OK] sortCommitsChronologically(ascending=false): oid4->oid3->oid2->oid1\n";

        // Original must be unchanged (function takes by value)
        ensure(mockCommits[0].oid == "oid4", "sortCommitsChronologically must not modify original vector");
        std::cout << "  [OK] sortCommitsChronologically did not modify original vector\n";
    }

    std::cout << "  PR #1 VERIFICATION PASSED (OOP BASE & REVISION CORE)\n";

    return 0;
}