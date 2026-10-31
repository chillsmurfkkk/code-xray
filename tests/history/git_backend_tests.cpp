#include "history/api.hpp"
#include "common/job.hpp"
#include "common/result.hpp"

#include <git2.h>

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stop_token>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

// Minimal RAII for libgit2 objects in test helper
template<class T, void(*Deleter)(T*)>
struct TestHandle {
    T* ptr = nullptr;
    TestHandle(T* p = nullptr) : ptr(p) {}
    ~TestHandle() { if (ptr) Deleter(ptr); }
    T* get() const { return ptr; }
    T** out() { return &ptr; }
};

using TRepoHandle      = TestHandle<git_repository,  git_repository_free>;
using TTreeHandle      = TestHandle<git_tree,        git_tree_free>;
using TTreeBldHandle   = TestHandle<git_treebuilder, git_treebuilder_free>;
using TCommitHandle    = TestHandle<git_commit,      git_commit_free>;
using TSignatureHandle = TestHandle<git_signature,   git_signature_free>;

std::string oidToHex(const git_oid& oid) {
    char buf[GIT_OID_SHA1_HEXSIZE + 1] = {};
    git_oid_tostr(buf, sizeof(buf), &oid);
    return buf;
}

// Controlled test repository fixture created via libgit2
struct TestGitRepo {
    fs::path repoPath;
    std::string rootCommitOid;
    std::string commit2Oid;
    std::string featCommitOid;
    std::string mergeCommitOid;
    std::string extraCommitOid;

    TestGitRepo() {
        git_libgit2_init();

        const auto uniqueSuffix = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()
        );
        repoPath = fs::temp_directory_path() / ("codexray_git_fixture_" + uniqueSuffix);
        fs::remove_all(repoPath);
        fs::create_directories(repoPath);

        TRepoHandle repo;
        if (git_repository_init(repo.out(), repoPath.string().c_str(), 0) != 0) {
            throw std::runtime_error("Failed to init test repository");
        }

        TSignatureHandle sig;
        git_signature_now(sig.out(), "Test Author", "author@example.com");

        // --- 1. Root Commit ---
        // Files: src/main.cpp, docs/readme.md, empty.txt
        git_oid mainCppBlob{}, readmeBlob{}, emptyBlob{};
        const char mainSrc[] = "#include <iostream>\nint main() { return 0; }\n";
        const char readmeSrc[] = "# Test Repo\nReadme file\n";
        const char emptySrc[] = "";
        git_blob_create_from_buffer(&mainCppBlob, repo.get(), mainSrc, sizeof(mainSrc) - 1);
        git_blob_create_from_buffer(&readmeBlob, repo.get(), readmeSrc, sizeof(readmeSrc) - 1);
        git_blob_create_from_buffer(&emptyBlob, repo.get(), emptySrc, 0);

        // Subtree: src/
        git_oid srcTreeOid{};
        {
            TTreeBldHandle bld;
            git_treebuilder_new(bld.out(), repo.get(), nullptr);
            git_treebuilder_insert(nullptr, bld.get(), "main.cpp", &mainCppBlob, GIT_FILEMODE_BLOB);
            git_treebuilder_write(&srcTreeOid, bld.get());
        }

        // Subtree: docs/
        git_oid docsTreeOid{};
        {
            TTreeBldHandle bld;
            git_treebuilder_new(bld.out(), repo.get(), nullptr);
            git_treebuilder_insert(nullptr, bld.get(), "readme.md", &readmeBlob, GIT_FILEMODE_BLOB);
            git_treebuilder_write(&docsTreeOid, bld.get());
        }

        // Root tree
        git_oid rootTreeOid{};
        {
            TTreeBldHandle bld;
            git_treebuilder_new(bld.out(), repo.get(), nullptr);
            git_treebuilder_insert(nullptr, bld.get(), "src", &srcTreeOid, GIT_FILEMODE_TREE);
            git_treebuilder_insert(nullptr, bld.get(), "docs", &docsTreeOid, GIT_FILEMODE_TREE);
            git_treebuilder_insert(nullptr, bld.get(), "empty.txt", &emptyBlob, GIT_FILEMODE_BLOB);
            git_treebuilder_write(&rootTreeOid, bld.get());
        }

        TTreeHandle rootTree;
        git_tree_lookup(rootTree.out(), repo.get(), &rootTreeOid);

        git_oid rootOid{};
        git_commit_create_v(&rootOid, repo.get(), "HEAD", sig.get(), sig.get(),
                            nullptr, "Initial root commit", rootTree.get(), 0);
        rootCommitOid = oidToHex(rootOid);

        TCommitHandle rootCommit;
        git_commit_lookup(rootCommit.out(), repo.get(), &rootOid);

        // --- 2. Commit 2 on main (parent: rootCommit) ---
        // Modifies src/main.cpp, adds src/helper.hpp, deletes empty.txt
        const char mainSrc2[] = "#include <iostream>\nint main() { std::cout << 1; return 0; }\n";
        const char helperSrc[] = "#pragma once\nvoid helper();\n";
        git_oid mainCppBlob2{}, helperBlob{};
        git_blob_create_from_buffer(&mainCppBlob2, repo.get(), mainSrc2, sizeof(mainSrc2) - 1);
        git_blob_create_from_buffer(&helperBlob, repo.get(), helperSrc, sizeof(helperSrc) - 1);

        git_oid srcTreeOid2{};
        {
            TTreeBldHandle bld;
            git_treebuilder_new(bld.out(), repo.get(), nullptr);
            git_treebuilder_insert(nullptr, bld.get(), "main.cpp", &mainCppBlob2, GIT_FILEMODE_BLOB);
            git_treebuilder_insert(nullptr, bld.get(), "helper.hpp", &helperBlob, GIT_FILEMODE_BLOB);
            git_treebuilder_write(&srcTreeOid2, bld.get());
        }

        git_oid treeOid2{};
        {
            TTreeBldHandle bld;
            git_treebuilder_new(bld.out(), repo.get(), nullptr);
            git_treebuilder_insert(nullptr, bld.get(), "src", &srcTreeOid2, GIT_FILEMODE_TREE);
            git_treebuilder_insert(nullptr, bld.get(), "docs", &docsTreeOid, GIT_FILEMODE_TREE);
            git_treebuilder_write(&treeOid2, bld.get());
        }

        TTreeHandle tree2;
        git_tree_lookup(tree2.out(), repo.get(), &treeOid2);

        git_oid c2Oid{};
        const git_commit* parents2[] = { rootCommit.get() };
        git_commit_create(&c2Oid, repo.get(), "HEAD", sig.get(), sig.get(),
                          nullptr, "Add helper and update main", tree2.get(), 1, parents2);
        commit2Oid = oidToHex(c2Oid);

        TCommitHandle c2Commit;
        git_commit_lookup(c2Commit.out(), repo.get(), &c2Oid);

        // --- 3. Branch commit on feature branch (parent: rootCommit) ---
        // Adds feature.cpp
        const char featSrc[] = "void feature() {}\n";
        git_oid featBlob{};
        git_blob_create_from_buffer(&featBlob, repo.get(), featSrc, sizeof(featSrc) - 1);

        git_oid treeOidFeat{};
        {
            TTreeBldHandle bld;
            git_treebuilder_new(bld.out(), repo.get(), nullptr);
            git_treebuilder_insert(nullptr, bld.get(), "src", &srcTreeOid, GIT_FILEMODE_TREE);
            git_treebuilder_insert(nullptr, bld.get(), "docs", &docsTreeOid, GIT_FILEMODE_TREE);
            git_treebuilder_insert(nullptr, bld.get(), "feature.cpp", &featBlob, GIT_FILEMODE_BLOB);
            git_treebuilder_write(&treeOidFeat, bld.get());
        }

        TTreeHandle treeFeat;
        git_tree_lookup(treeFeat.out(), repo.get(), &treeOidFeat);

        git_oid featOid{};
        const git_commit* parentsFeat[] = { rootCommit.get() };
        git_commit_create(&featOid, repo.get(), nullptr, sig.get(), sig.get(),
                          nullptr, "Feature branch commit", treeFeat.get(), 1, parentsFeat);
        featCommitOid = oidToHex(featOid);

        TCommitHandle featCommit;
        git_commit_lookup(featCommit.out(), repo.get(), &featOid);

        // --- 4. Merge Commit on main (first-parent: c2Commit, second-parent: featCommit) ---
        git_oid treeOidMerge{};
        {
            TTreeBldHandle bld;
            git_treebuilder_new(bld.out(), repo.get(), nullptr);
            git_treebuilder_insert(nullptr, bld.get(), "src", &srcTreeOid2, GIT_FILEMODE_TREE);
            git_treebuilder_insert(nullptr, bld.get(), "docs", &docsTreeOid, GIT_FILEMODE_TREE);
            git_treebuilder_insert(nullptr, bld.get(), "feature.cpp", &featBlob, GIT_FILEMODE_BLOB);
            git_treebuilder_write(&treeOidMerge, bld.get());
        }

        TTreeHandle treeMerge;
        git_tree_lookup(treeMerge.out(), repo.get(), &treeOidMerge);

        git_oid mergeOid{};
        const git_commit* parentsMerge[] = { c2Commit.get(), featCommit.get() };
        git_commit_create(&mergeOid, repo.get(), "HEAD", sig.get(), sig.get(),
                          nullptr, "Merge branch feature into main", treeMerge.get(), 2, parentsMerge);
        mergeCommitOid = oidToHex(mergeOid);

        TCommitHandle mergeCommit;
        git_commit_lookup(mergeCommit.out(), repo.get(), &mergeOid);

        // --- 5. Extra commit: nested vendor/, Windows-1251 file, literal pathspec foo[1].cpp, foo1.cpp, binary.bin ---
        // Binary blob
        const char binData[] = { 'B', 'I', 'N', '\0', 'D', 'A', 'T', 'A' };
        git_oid binBlob{};
        git_blob_create_from_buffer(&binBlob, repo.get(), binData, sizeof(binData));

        // Windows-1251 text (invalid UTF-8): "Привет" in CP1251 is \xCF\xF0\xE8\xE2\xE5\xF2 (no NUL bytes)
        const char cp1251Data[] = { '\xCF', '\xF0', '\xE8', '\xE2', '\xE5', '\xF2', '\n' };
        git_oid cp1251Blob{};
        git_blob_create_from_buffer(&cp1251Blob, repo.get(), cp1251Data, sizeof(cp1251Data));

        // Literal wildcard test files
        const char fooBracketSrc[] = "// foo[1].cpp\n";
        const char foo1Src[] = "// foo1.cpp\n";
        git_oid fooBracketBlob{}, foo1Blob{};
        git_blob_create_from_buffer(&fooBracketBlob, repo.get(), fooBracketSrc, sizeof(fooBracketSrc) - 1);
        git_blob_create_from_buffer(&foo1Blob, repo.get(), foo1Src, sizeof(foo1Src) - 1);

        // Nested vendor directory: src/vendor/vendored.cpp
        const char vendorSrc[] = "void vendored() {}\n";
        git_oid vendorBlob{}, vendorTreeOid{};
        git_blob_create_from_buffer(&vendorBlob, repo.get(), vendorSrc, sizeof(vendorSrc) - 1);
        {
            TTreeBldHandle bld;
            git_treebuilder_new(bld.out(), repo.get(), nullptr);
            git_treebuilder_insert(nullptr, bld.get(), "vendored.cpp", &vendorBlob, GIT_FILEMODE_BLOB);
            git_treebuilder_write(&vendorTreeOid, bld.get());
        }

        git_oid srcTreeOid3{};
        {
            TTreeBldHandle bld;
            git_treebuilder_new(bld.out(), repo.get(), nullptr);
            git_treebuilder_insert(nullptr, bld.get(), "main.cpp", &mainCppBlob2, GIT_FILEMODE_BLOB);
            git_treebuilder_insert(nullptr, bld.get(), "helper.hpp", &helperBlob, GIT_FILEMODE_BLOB);
            git_treebuilder_insert(nullptr, bld.get(), "vendor", &vendorTreeOid, GIT_FILEMODE_TREE);
            git_treebuilder_insert(nullptr, bld.get(), "foo[1].cpp", &fooBracketBlob, GIT_FILEMODE_BLOB);
            git_treebuilder_insert(nullptr, bld.get(), "foo1.cpp", &foo1Blob, GIT_FILEMODE_BLOB);
            git_treebuilder_write(&srcTreeOid3, bld.get());
        }

        git_oid assetsTreeOid{};
        {
            TTreeBldHandle bld;
            git_treebuilder_new(bld.out(), repo.get(), nullptr);
            git_treebuilder_insert(nullptr, bld.get(), "binary.bin", &binBlob, GIT_FILEMODE_BLOB);
            git_treebuilder_insert(nullptr, bld.get(), "win1251.cpp", &cp1251Blob, GIT_FILEMODE_BLOB);
            git_treebuilder_write(&assetsTreeOid, bld.get());
        }

        git_oid treeOidExtra{};
        {
            TTreeBldHandle bld;
            git_treebuilder_new(bld.out(), repo.get(), nullptr);
            git_treebuilder_insert(nullptr, bld.get(), "src", &srcTreeOid3, GIT_FILEMODE_TREE);
            git_treebuilder_insert(nullptr, bld.get(), "docs", &docsTreeOid, GIT_FILEMODE_TREE);
            git_treebuilder_insert(nullptr, bld.get(), "feature.cpp", &featBlob, GIT_FILEMODE_BLOB);
            git_treebuilder_insert(nullptr, bld.get(), "assets", &assetsTreeOid, GIT_FILEMODE_TREE);
            git_treebuilder_write(&treeOidExtra, bld.get());
        }

        TTreeHandle treeExtra;
        git_tree_lookup(treeExtra.out(), repo.get(), &treeOidExtra);

        git_oid extraOid{};
        const git_commit* parentsExtra[] = { mergeCommit.get() };
        git_commit_create(&extraOid, repo.get(), "HEAD", sig.get(), sig.get(),
                          nullptr, "Add vendor, win1251 and bracket files", treeExtra.get(), 1, parentsExtra);
        extraCommitOid = oidToHex(extraOid);
    }

    ~TestGitRepo() {
        std::error_code ec;
        fs::remove_all(repoPath, ec);
        git_libgit2_shutdown();
    }
};

int gPass = 0;
int gFail = 0;

void check(bool condition, const char* desc) {
    if (condition) {
        std::cout << "  [PASS] " << desc << "\n";
        ++gPass;
    } else {
        std::cerr << "  [FAIL] " << desc << "\n";
        ++gFail;
    }
}

// 1. Test strictly first-parent traversal
void testFirstParentTraversal(const TestGitRepo& fixture) {
    std::cout << "--- testFirstParentTraversal ---\n";
    xray::JobContext ctx;
    xray::history::RepositorySpec spec{ fixture.repoPath };
    xray::history::HistoryRequest req;
    req.startOid = fixture.mergeCommitOid; // start from merge commit
    req.limit = 10;

    auto res = xray::history::query(spec, req, ctx);
    check(std::holds_alternative<xray::history::HistoryResult>(res), "query succeeded");
    if (!std::holds_alternative<xray::history::HistoryResult>(res)) return;

    const auto& hist = std::get<xray::history::HistoryResult>(res);
    check(hist.commits.size() == 3, "commits count is 3 (merge -> c2 -> root)");
    if (hist.commits.size() >= 3) {
        check(hist.commits[0].oid == fixture.mergeCommitOid, "first commit is merge");
        check(hist.commits[1].oid == fixture.commit2Oid, "second commit is c2 (first-parent)");
        check(hist.commits[2].oid == fixture.rootCommitOid, "third commit is root");
    }

    bool foundFeatureBranchCommit = false;
    for (const auto& c : hist.commits) {
        if (c.oid == fixture.featCommitOid) foundFeatureBranchCommit = true;
    }
    check(!foundFeatureBranchCommit, "feature branch commit excluded from first-parent walk");
}

// 2. Test second parent selection in a multi-commit history (does NOT crash on subsequent single-parent commits)
void testSecondParentInMultiCommitHistory(const TestGitRepo& fixture) {
    std::cout << "--- testSecondParentInMultiCommitHistory ---\n";
    xray::JobContext ctx;
    xray::history::RepositorySpec spec{ fixture.repoPath };
    xray::history::HistoryRequest req;
    req.startOid = fixture.mergeCommitOid;
    req.parentIndex = 1; // choose second parent for merge commit
    req.limit = 10;      // continue past merge to single-parent commit c2 and root

    auto res = xray::history::query(spec, req, ctx);
    check(std::holds_alternative<xray::history::HistoryResult>(res),
          "query with parentIndex=1 across multi-commit history succeeded");
    if (!std::holds_alternative<xray::history::HistoryResult>(res)) return;

    const auto& hist = std::get<xray::history::HistoryResult>(res);
    check(hist.commits.size() == 3, "traversed all 3 first-parent commits without error");

    // For the merge commit, parentOid in changes should match feature branch commit
    bool mergeDiffAgainstSecondParent = false;
    for (const auto& ch : hist.changes) {
        if (ch.commitOid == fixture.mergeCommitOid && ch.parentOid == fixture.featCommitOid) {
            mergeDiffAgainstSecondParent = true;
            break;
        }
    }
    check(mergeDiffAgainstSecondParent, "merge commit diffed against second parent");
}

// 3. Test relativePath filtering: commits not touching path are skipped & do not consume limit
void testRelativePathFiltering(const TestGitRepo& fixture) {
    std::cout << "--- testRelativePathFiltering ---\n";
    xray::JobContext ctx;
    xray::history::RepositorySpec spec{ fixture.repoPath };
    xray::history::HistoryRequest req;
    req.startOid = fixture.mergeCommitOid;
    req.relativePath = "src/helper.hpp";
    req.limit = 5;

    auto res = xray::history::query(spec, req, ctx);
    check(std::holds_alternative<xray::history::HistoryResult>(res), "query with relativePath succeeded");
    if (!std::holds_alternative<xray::history::HistoryResult>(res)) return;

    const auto& hist = std::get<xray::history::HistoryResult>(res);
    check(hist.commits.size() == 1, "exactly 1 commit returned (c2 where helper.hpp was added)");
    if (!hist.commits.empty()) {
        check(hist.commits[0].oid == fixture.commit2Oid, "commit is c2");
    }
    check(!hist.changes.empty(), "changes populated for helper.hpp");
    if (!hist.changes.empty()) {
        check(hist.changes[0].newPath.has_value() && *hist.changes[0].newPath == "src/helper.hpp",
              "change matches filtered path");
    }
}

// 4. Test literal pathspec without wildcard matching
void testLiteralPathspec(const TestGitRepo& fixture) {
    std::cout << "--- testLiteralPathspec ---\n";
    xray::JobContext ctx;
    xray::history::RepositorySpec spec{ fixture.repoPath };
    xray::history::HistoryRequest req;
    req.startOid = fixture.extraCommitOid;
    req.relativePath = "src/foo[1].cpp"; // contains brackets []
    req.limit = 5;

    auto res = xray::history::query(spec, req, ctx);
    check(std::holds_alternative<xray::history::HistoryResult>(res), "literal path query succeeded");
    if (!std::holds_alternative<xray::history::HistoryResult>(res)) return;

    const auto& hist = std::get<xray::history::HistoryResult>(res);
    check(hist.commits.size() == 1, "exactly 1 commit returned for literal foo[1].cpp");

    bool matchedFooBracket = false;
    bool matchedFoo1 = false;
    for (const auto& ch : hist.changes) {
        if (ch.newPath == "src/foo[1].cpp") matchedFooBracket = true;
        if (ch.newPath == "src/foo1.cpp")    matchedFoo1 = true;
    }
    check(matchedFooBracket, "change matched exact path src/foo[1].cpp");
    check(!matchedFoo1, "foo1.cpp was NOT matched by wildcard expansion");
}

// 5. Test hasMore accuracy for file history
void testHasMoreAccuracyForFile(const TestGitRepo& fixture) {
    std::cout << "--- testHasMoreAccuracyForFile ---\n";
    xray::JobContext ctx;
    xray::history::RepositorySpec spec{ fixture.repoPath };

    // Case A: src/helper.hpp was only added in c2. When queried from mergeCommit with limit=1,
    // c2 is returned and there are NO older commits modifying helper.hpp -> hasMore MUST be false!
    {
        xray::history::HistoryRequest req;
        req.startOid = fixture.mergeCommitOid;
        req.relativePath = "src/helper.hpp";
        req.limit = 1;

        auto res = xray::history::query(spec, req, ctx);
        check(std::holds_alternative<xray::history::HistoryResult>(res), "query succeeded");
        if (std::holds_alternative<xray::history::HistoryResult>(res)) {
            const auto& hist = std::get<xray::history::HistoryResult>(res);
            check(hist.commits.size() == 1, "found c2");
            check(hist.hasMore == false, "hasMore is false because no older commit modified helper.hpp");
        }
    }

    // Case B: src/main.cpp was modified in c2 AND created in root.
    // When queried from c2 with limit=1, root is an older commit that also modified it -> hasMore MUST be true!
    {
        xray::history::HistoryRequest req;
        req.startOid = fixture.commit2Oid;
        req.relativePath = "src/main.cpp";
        req.limit = 1;

        auto res = xray::history::query(spec, req, ctx);
        check(std::holds_alternative<xray::history::HistoryResult>(res), "query succeeded");
        if (std::holds_alternative<xray::history::HistoryResult>(res)) {
            const auto& hist = std::get<xray::history::HistoryResult>(res);
            check(hist.commits.size() == 1, "found 1 commit");
            check(hist.hasMore == true, "hasMore is true because older commit root modified main.cpp");
        }
    }
}

// 6. Test nested excluded directories in loadSnapshot
void testNestedExcludedDirectory(const TestGitRepo& fixture) {
    std::cout << "--- testNestedExcludedDirectory ---\n";
    xray::JobContext ctx;
    xray::history::RepositorySpec spec{ fixture.repoPath };
    xray::code::FileSelection sel;
    sel.relativePaths = { "src" };
    sel.extensions = { ".cpp", ".hpp" };
    sel.excludedDirectories = { "vendor" }; // exclude directory named "vendor" at ANY depth

    auto res = xray::history::loadSnapshot(spec, fixture.extraCommitOid, sel, ctx);
    check(std::holds_alternative<xray::code::SourceSnapshot>(res), "loadSnapshot succeeded");
    if (!std::holds_alternative<xray::code::SourceSnapshot>(res)) return;

    const auto& snap = std::get<xray::code::SourceSnapshot>(res);
    check(snap.coverage.completeness == xray::Completeness::complete,
          "coverage is complete (nested exclusion is normal filtering)");

    bool foundVendored = false;
    for (const auto& f : snap.files) {
        if (f.relativePath == "src/vendor/vendored.cpp") foundVendored = true;
    }
    check(!foundVendored, "src/vendor/vendored.cpp successfully excluded by nested directory filter");
}

// 7. Test non-UTF8 encoding detection in loadSnapshot
void testNonUtf8EncodingDetection(const TestGitRepo& fixture) {
    std::cout << "--- testNonUtf8EncodingDetection ---\n";
    xray::JobContext ctx;
    xray::history::RepositorySpec spec{ fixture.repoPath };
    xray::code::FileSelection sel;
    sel.relativePaths = { "assets/win1251.cpp" };

    auto res = xray::history::loadSnapshot(spec, fixture.extraCommitOid, sel, ctx);
    check(std::holds_alternative<xray::code::SourceSnapshot>(res), "loadSnapshot succeeded");
    if (!std::holds_alternative<xray::code::SourceSnapshot>(res)) return;

    const auto& snap = std::get<xray::code::SourceSnapshot>(res);
    check(snap.coverage.completeness == xray::Completeness::partial,
          "coverage is partial due to non-UTF8 file");
    check(snap.coverage.skippedElements == 1, "1 skipped element for non-UTF8");
    check(!snap.coverage.diagnostics.empty(), "diagnostic recorded");
    if (!snap.coverage.diagnostics.empty()) {
        const auto& d = snap.coverage.diagnostics[0];
        check(d.code == xray::ErrorCode::unsupported_encoding, "diagnostic is unsupported_encoding");
        check(d.path.has_value() && *d.path == "assets/win1251.cpp", "diagnostic path matches");
    }
}

// 8. Test parentIndex validation
void testParentIndexValidation(const TestGitRepo& fixture) {
    std::cout << "--- testParentIndexValidation ---\n";
    xray::JobContext ctx;
    xray::history::RepositorySpec spec{ fixture.repoPath };

    // Case A: Root commit with parentIndex = 1 -> must fail with invalid_input
    {
        xray::history::HistoryRequest req;
        req.startOid = fixture.rootCommitOid;
        req.limit = 1;
        req.parentIndex = 1;

        auto res = xray::history::query(spec, req, ctx);
        check(std::holds_alternative<xray::Error>(res), "root with parentIndex=1 rejected as Error");
        if (std::holds_alternative<xray::Error>(res)) {
            check(std::get<xray::Error>(res).code == xray::ErrorCode::invalid_input,
                  "error code is invalid_input");
        }
    }

    // Case B: Merge commit (2 parents) with parentIndex = 5 -> must fail with invalid_input
    {
        xray::history::HistoryRequest req;
        req.startOid = fixture.mergeCommitOid;
        req.limit = 1;
        req.parentIndex = 5;

        auto res = xray::history::query(spec, req, ctx);
        check(std::holds_alternative<xray::Error>(res), "merge with parentIndex=5 rejected as Error");
        if (std::holds_alternative<xray::Error>(res)) {
            check(std::get<xray::Error>(res).code == xray::ErrorCode::invalid_input,
                  "error code is invalid_input");
        }
    }
}

// 9. Test cancellation
void testCancellation(const TestGitRepo& fixture) {
    std::cout << "--- testCancellation ---\n";
    std::stop_source stopSrc;
    stopSrc.request_stop();

    xray::JobContext ctx;
    ctx.stopToken = stopSrc.get_token();

    xray::history::RepositorySpec spec{ fixture.repoPath };
    xray::history::HistoryRequest req;
    auto res = xray::history::query(spec, req, ctx);
    check(std::holds_alternative<xray::Cancelled>(res), "cancelled query returns Cancelled");

    xray::code::FileSelection sel;
    auto snapRes = xray::history::loadSnapshot(spec, fixture.rootCommitOid, sel, ctx);
    check(std::holds_alternative<xray::Cancelled>(snapRes), "cancelled loadSnapshot returns Cancelled");
}

} // namespace

int main() {
    try {
        std::cout << "Initializing TestGitRepo fixture...\n";
        TestGitRepo fixture;
        std::cout << "TestGitRepo created successfully.\n";

        testFirstParentTraversal(fixture);
        testSecondParentInMultiCommitHistory(fixture);
        testRelativePathFiltering(fixture);
        testLiteralPathspec(fixture);
        testHasMoreAccuracyForFile(fixture);
        testNestedExcludedDirectory(fixture);
        testNonUtf8EncodingDetection(fixture);
        testParentIndexValidation(fixture);
        testCancellation(fixture);

        std::cout << "\n========================================\n";
        std::cout << "Summary: " << gPass << " passed, " << gFail << " failed.\n";
        std::cout << "========================================\n";
        return (gFail == 0) ? 0 : 1;
    } catch (const std::exception& ex) {
        std::cerr << "Fatal exception during tests: " << ex.what() << "\n";
        return 2;
    }
}
