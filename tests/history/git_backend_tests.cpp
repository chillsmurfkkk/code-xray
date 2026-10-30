// git_backend_tests.cpp
// Tests for the libgit2-backed xray::history::query() and loadSnapshot().
// The tests run against the code-xray repository itself, which is always
// present next to the test binary during CI/local builds.

#include "history/api.hpp"
#include "code/types.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>
#include <variant>

using namespace xray;
using namespace xray::history;
namespace fs = std::filesystem;

// -------------------------------------------------------------------------
// Minimal assertion helpers (no external test framework needed)
// -------------------------------------------------------------------------
static int g_failures = 0;
#define EXPECT(expr) \
    do { if (!(expr)) { \
        std::cerr << "[FAIL] " #expr " at " __FILE__ ":" << __LINE__ << "\n"; \
        ++g_failures; \
    } else { \
        std::cout << "[OK]   " #expr "\n"; \
    } } while(0)

#define EXPECT_MSG(expr, msg) \
    do { if (!(expr)) { \
        std::cerr << "[FAIL] " << (msg) << " at " __FILE__ ":" << __LINE__ << "\n"; \
        ++g_failures; \
    } else { \
        std::cout << "[OK]   " << (msg) << "\n"; \
    } } while(0)

// -------------------------------------------------------------------------
// Find the repository root: walk up from the executable location
// -------------------------------------------------------------------------
static fs::path findRepoRoot() {
    // Executable is in build/windows-x64/Debug (or similar), go up to find .git
    fs::path candidate = fs::current_path();
    for (int i = 0; i < 6; ++i) {
        if (fs::exists(candidate / ".git")) return candidate;
        if (candidate.has_parent_path()) candidate = candidate.parent_path();
    }
    // Fallback: look relative to this source file's compile-time directory
    candidate = fs::path(__FILE__).parent_path();
    for (int i = 0; i < 5; ++i) {
        if (fs::exists(candidate / ".git")) return candidate;
        if (candidate.has_parent_path()) candidate = candidate.parent_path();
    }
    return {};
}

// -------------------------------------------------------------------------
// Tests
// -------------------------------------------------------------------------
static void testQueryHeadCommits(const RepositorySpec& spec) {
    std::cout << "\n=== testQueryHeadCommits ===\n";

    HistoryRequest req;
    req.limit = 5;
    JobContext ctx;

    auto result = query(spec, req, ctx);

    EXPECT(std::holds_alternative<HistoryResult>(result));
    if (!std::holds_alternative<HistoryResult>(result)) return;

    const auto& hr = std::get<HistoryResult>(result);
    EXPECT_MSG(!hr.commits.empty(), "query() returned at least one commit");
    EXPECT_MSG(hr.commits.size() <= 5, "query() respects limit=5");

    const auto& c = hr.commits.front();
    EXPECT_MSG(!c.oid.empty(), "first commit has non-empty OID");
    EXPECT_MSG(c.oid.size() == 40, "OID is 40 hex chars: " + c.oid);
    EXPECT_MSG(!c.authorName.empty(), "first commit has authorName");
    EXPECT_MSG(!c.authorEmail.empty(), "first commit has authorEmail");
    EXPECT_MSG(c.authoredAt.time_since_epoch().count() > 0, "authoredAt > 0");
    EXPECT_MSG(c.committedAt.time_since_epoch().count() > 0, "committedAt > 0");
    EXPECT_MSG(!c.message.empty(), "first commit has message");

    std::cout << "  HEAD OID: " << c.oid << "\n";
    std::cout << "  Author:   " << c.authorName << " <" << c.authorEmail << ">\n";
    std::cout << "  Message:  " << c.message.substr(0, 72) << "\n";
}

static void testQueryChangesPopulated(const RepositorySpec& spec) {
    std::cout << "\n=== testQueryChangesPopulated ===\n";

    HistoryRequest req;
    req.limit = 10;
    JobContext ctx;

    auto result = query(spec, req, ctx);
    EXPECT(std::holds_alternative<HistoryResult>(result));
    if (!std::holds_alternative<HistoryResult>(result)) return;

    const auto& hr = std::get<HistoryResult>(result);
    // The repository has history with file changes
    EXPECT_MSG(!hr.changes.empty(), "query() collected at least one ChangeRecord");

    // Every ChangeRecord must have a valid commitOid
    bool allHaveOid = true;
    for (const auto& ch : hr.changes) {
        if (ch.commitOid.empty()) { allHaveOid = false; break; }
    }
    EXPECT_MSG(allHaveOid, "every ChangeRecord has a non-empty commitOid");

    std::cout << "  Total ChangeRecords from last 10 commits: " << hr.changes.size() << "\n";
}

static void testQueryWithStartOid(const RepositorySpec& spec) {
    std::cout << "\n=== testQueryWithStartOid ===\n";

    // First fetch HEAD OID
    HistoryRequest headReq;
    headReq.limit = 1;
    JobContext ctx;
    auto headResult = query(spec, headReq, ctx);
    if (!std::holds_alternative<HistoryResult>(headResult)) {
        std::cerr << "[SKIP] Could not read HEAD for startOid test\n";
        return;
    }
    const std::string headOid = std::get<HistoryResult>(headResult).commits.front().oid;

    HistoryRequest req;
    req.startOid = headOid;
    req.limit = 3;

    auto result = query(spec, req, ctx);
    EXPECT(std::holds_alternative<HistoryResult>(result));
    if (!std::holds_alternative<HistoryResult>(result)) return;

    const auto& hr = std::get<HistoryResult>(result);
    EXPECT_MSG(!hr.commits.empty(), "query(startOid=HEAD) returned commits");
    EXPECT_MSG(hr.commits.front().oid == headOid, "first commit is the requested startOid");

    std::cout << "  Commits from startOid " << headOid.substr(0, 8) << "...: "
              << hr.commits.size() << "\n";
}

static void testQueryInvalidOidReturnsError(const RepositorySpec& spec) {
    std::cout << "\n=== testQueryInvalidOidReturnsError ===\n";

    HistoryRequest req;
    req.startOid = "not-a-valid-oid";
    req.limit = 1;
    JobContext ctx;

    auto result = query(spec, req, ctx);
    EXPECT_MSG(std::holds_alternative<Error>(result),
               "query() with invalid OID returns xray::Error");
    if (std::holds_alternative<Error>(result)) {
        std::cout << "  Error: " << std::get<Error>(result).message << "\n";
    }
}

static void testQueryNonExistentRepo() {
    std::cout << "\n=== testQueryNonExistentRepo ===\n";

    RepositorySpec badSpec{ fs::path("C:/totally/does/not/exist/code-xray") };
    HistoryRequest req;
    req.limit = 1;
    JobContext ctx;

    auto result = query(badSpec, req, ctx);
    EXPECT_MSG(std::holds_alternative<Error>(result),
               "query() on non-existent repo returns xray::Error");
    if (std::holds_alternative<Error>(result)) {
        std::cout << "  Error: " << std::get<Error>(result).message << "\n";
    }
}

static void testQueryHasMore(const RepositorySpec& spec) {
    std::cout << "\n=== testQueryHasMore ===\n";

    // Ask for just 1 commit; if there are more, hasMore must be true
    HistoryRequest req;
    req.limit = 1;
    JobContext ctx;
    auto result = query(spec, req, ctx);
    EXPECT(std::holds_alternative<HistoryResult>(result));
    if (!std::holds_alternative<HistoryResult>(result)) return;

    const auto& hr = std::get<HistoryResult>(result);
    // code-xray has many commits, so hasMore must be true for limit=1
    EXPECT_MSG(hr.hasMore, "hasMore=true when limit=1 on a repo with many commits");
}

static void testQueryParentOids(const RepositorySpec& spec) {
    std::cout << "\n=== testQueryParentOids ===\n";

    HistoryRequest req;
    req.limit = 20;
    JobContext ctx;
    auto result = query(spec, req, ctx);
    EXPECT(std::holds_alternative<HistoryResult>(result));
    if (!std::holds_alternative<HistoryResult>(result)) return;

    const auto& hr = std::get<HistoryResult>(result);
    // Every commit except the very first should have at least one parent OID,
    // and the parent OID must be 40 hex chars
    bool allValid = true;
    for (std::size_t i = 0; i < hr.commits.size(); ++i) {
        for (const auto& pOid : hr.commits[i].parentOids) {
            if (pOid.size() != 40) { allValid = false; break; }
        }
    }
    EXPECT_MSG(allValid, "all parentOids are 40-char hex strings");
}

static void testLoadSnapshot(const RepositorySpec& spec) {
    std::cout << "\n=== testLoadSnapshot ===\n";

    // Get HEAD OID first
    HistoryRequest headReq;
    headReq.limit = 1;
    JobContext ctx;
    auto headResult = query(spec, headReq, ctx);
    if (!std::holds_alternative<HistoryResult>(headResult)) {
        std::cerr << "[SKIP] Could not read HEAD for loadSnapshot test\n";
        return;
    }
    const std::string headOid = std::get<HistoryResult>(headResult).commits.front().oid;

    code::FileSelection sel;
    // Keep defaults: .cpp/.h/.hpp/.hxx etc., exclude .git/build/vendor

    auto snapResult = loadSnapshot(spec, headOid, sel, ctx);
    EXPECT(std::holds_alternative<code::SourceSnapshot>(snapResult));
    if (!std::holds_alternative<code::SourceSnapshot>(snapResult)) return;

    const auto& snap = std::get<code::SourceSnapshot>(snapResult);
    EXPECT_MSG(snap.kind == code::SourceKind::git, "SourceSnapshot::kind == git");
    EXPECT_MSG(snap.commitOid.has_value(), "commitOid is populated");
    EXPECT_MSG(*snap.commitOid == headOid, "commitOid matches requested OID");
    EXPECT_MSG(!snap.files.empty(), "loadSnapshot found at least one file");

    // Every file must have a non-empty relativePath and non-null bytes
    bool allValid = true;
    for (const auto& f : snap.files) {
        if (f.relativePath.empty() || !f.bytes) { allValid = false; break; }
        if (f.contentId.empty())                  { allValid = false; break; }
    }
    EXPECT_MSG(allValid, "all SourceFiles have relativePath, bytes, and contentId");

    std::cout << "  Files in snapshot from " << headOid.substr(0, 8) << ": "
              << snap.files.size() << "\n";
    for (std::size_t i = 0; i < std::min(snap.files.size(), std::size_t{5}); ++i) {
        std::cout << "    " << snap.files[i].relativePath << "\n";
    }
}

static void testLoadSnapshotBadOid(const RepositorySpec& spec) {
    std::cout << "\n=== testLoadSnapshotBadOid ===\n";

    JobContext ctx;
    code::FileSelection sel;

    auto result = loadSnapshot(spec, "deadbeefdeadbeefdeadbeefdeadbeefdeadbeef", sel, ctx);
    EXPECT_MSG(std::holds_alternative<Error>(result),
               "loadSnapshot with non-existent OID returns xray::Error");
    if (std::holds_alternative<Error>(result)) {
        std::cout << "  Error: " << std::get<Error>(result).message << "\n";
    }
}

static void testLoadSnapshotFileSelectionFiltering(const RepositorySpec& spec) {
    std::cout << "\n=== testLoadSnapshotFileSelectionFiltering ===\n";

    HistoryRequest headReq;
    headReq.limit = 1;
    JobContext ctx;
    auto headResult = query(spec, headReq, ctx);
    if (!std::holds_alternative<HistoryResult>(headResult)) {
        std::cerr << "[SKIP] Could not read HEAD\n"; return;
    }
    const std::string headOid = std::get<HistoryResult>(headResult).commits.front().oid;

    // Select only .md files
    code::FileSelection selMd;
    selMd.extensions = { ".md" };

    auto snapMd = loadSnapshot(spec, headOid, selMd, ctx);
    EXPECT(std::holds_alternative<code::SourceSnapshot>(snapMd));
    if (!std::holds_alternative<code::SourceSnapshot>(snapMd)) return;
    const auto& mdFiles = std::get<code::SourceSnapshot>(snapMd).files;

    bool allMd = true;
    for (const auto& f : mdFiles) {
        if (!f.relativePath.ends_with(".md")) { allMd = false; break; }
    }
    EXPECT_MSG(allMd, "FileSelection{.md} returns only .md files");
    EXPECT_MSG(!mdFiles.empty(), "there is at least one .md file in HEAD");

    std::cout << "  .md files found: " << mdFiles.size() << "\n";
}

// -------------------------------------------------------------------------
// main
// -------------------------------------------------------------------------
int main() {
    std::cout << "  CodeXray: libgit2 Backend Tests\n";

    const fs::path repoRoot = findRepoRoot();
    if (repoRoot.empty() || !fs::exists(repoRoot / ".git")) {
        std::cerr << "[ERROR] Could not locate code-xray .git directory. "
                  << "Run tests from the project root or build directory.\n";
        return 2;
    }
    std::cout << "  Repository root: " << repoRoot.string() << "\n";

    const RepositorySpec spec{ repoRoot };

    testQueryHeadCommits(spec);
    testQueryChangesPopulated(spec);
    testQueryWithStartOid(spec);
    testQueryInvalidOidReturnsError(spec);
    testQueryNonExistentRepo();
    testQueryHasMore(spec);
    testQueryParentOids(spec);
    testLoadSnapshot(spec);
    testLoadSnapshotBadOid(spec);
    testLoadSnapshotFileSelectionFiltering(spec);

    std::cout << "\n";
    if (g_failures == 0) {
        std::cout << "  All git_backend tests PASSED\n";
        return 0;
    } else {
        std::cerr << "  " << g_failures << " test(s) FAILED\n";
        return 1;
    }
}
