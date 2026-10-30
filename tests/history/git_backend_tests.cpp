#include "history/api.hpp"
#include "common/job.hpp"
#include "common/result.hpp"

#include <git2.h>

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
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
    std::string binaryCommitOid;

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
        // Contains src/main.cpp, src/helper.hpp, feature.cpp, docs/readme.md
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

        // --- 5. Commit with Binary File (parent: mergeCommit) ---
        const char binData[] = { 'B', 'I', 'N', '\0', 'D', 'A', 'T', 'A', '\0' };
        git_oid binBlob{};
        git_blob_create_from_buffer(&binBlob, repo.get(), binData, sizeof(binData));

        git_oid assetsTreeOid{};
        {
            TTreeBldHandle bld;
            git_treebuilder_new(bld.out(), repo.get(), nullptr);
            git_treebuilder_insert(nullptr, bld.get(), "binary.bin", &binBlob, GIT_FILEMODE_BLOB);
            git_treebuilder_write(&assetsTreeOid, bld.get());
        }

        git_oid treeOidBin{};
        {
            TTreeBldHandle bld;
            git_treebuilder_new(bld.out(), repo.get(), nullptr);
            git_treebuilder_insert(nullptr, bld.get(), "src", &srcTreeOid2, GIT_FILEMODE_TREE);
            git_treebuilder_insert(nullptr, bld.get(), "docs", &docsTreeOid, GIT_FILEMODE_TREE);
            git_treebuilder_insert(nullptr, bld.get(), "feature.cpp", &featBlob, GIT_FILEMODE_BLOB);
            git_treebuilder_insert(nullptr, bld.get(), "assets", &assetsTreeOid, GIT_FILEMODE_TREE);
            git_treebuilder_write(&treeOidBin, bld.get());
        }

        TTreeHandle treeBin;
        git_tree_lookup(treeBin.out(), repo.get(), &treeOidBin);

        git_oid binOid{};
        const git_commit* parentsBin[] = { mergeCommit.get() };
        git_commit_create(&binOid, repo.get(), "HEAD", sig.get(), sig.get(),
                          nullptr, "Add binary assets", treeBin.get(), 1, parentsBin);
        binaryCommitOid = oidToHex(binOid);
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

    // Verify feature branch commit is NOT present in first-parent traversal
    bool foundFeatureBranchCommit = false;
    for (const auto& c : hist.commits) {
        if (c.oid == fixture.featCommitOid) foundFeatureBranchCommit = true;
    }
    check(!foundFeatureBranchCommit, "feature branch commit excluded from first-parent walk");
}

// 2. Test relativePath filtering: commits not touching path are skipped & do not consume limit
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

// 3. Test parentIndex validation
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

    // Case C: Merge commit with parentIndex = 0 (first parent)
    {
        xray::history::HistoryRequest req;
        req.startOid = fixture.mergeCommitOid;
        req.limit = 1;
        req.parentIndex = 0;

        auto res = xray::history::query(spec, req, ctx);
        check(std::holds_alternative<xray::history::HistoryResult>(res), "merge with parentIndex=0 succeeded");
    }

    // Case D: Merge commit with parentIndex = 1 (second parent)
    {
        xray::history::HistoryRequest req;
        req.startOid = fixture.mergeCommitOid;
        req.limit = 1;
        req.parentIndex = 1;

        auto res = xray::history::query(spec, req, ctx);
        check(std::holds_alternative<xray::history::HistoryResult>(res), "merge with parentIndex=1 succeeded");
    }
}

// 4. Test loadSnapshot: normal filtering is complete, empty file included
void testSnapshotTextAndEmptyFile(const TestGitRepo& fixture) {
    std::cout << "--- testSnapshotTextAndEmptyFile ---\n";
    xray::JobContext ctx;
    xray::history::RepositorySpec spec{ fixture.repoPath };
    xray::code::FileSelection sel;
    // Request only .cpp and .txt files; docs/readme.md is outside selection
    sel.extensions = { ".cpp", ".txt" };

    auto res = xray::history::loadSnapshot(spec, fixture.rootCommitOid, sel, ctx);
    check(std::holds_alternative<xray::code::SourceSnapshot>(res), "loadSnapshot succeeded");
    if (!std::holds_alternative<xray::code::SourceSnapshot>(res)) return;

    const auto& snap = std::get<xray::code::SourceSnapshot>(res);
    check(snap.coverage.completeness == xray::Completeness::complete,
          "coverage is complete despite readme.md filtered out");
    check(snap.coverage.skippedElements == 0, "skippedElements is 0");
    check(snap.coverage.diagnostics.empty(), "no error diagnostics");

    bool foundMainCpp = false;
    bool foundEmptyTxt = false;
    for (const auto& f : snap.files) {
        if (f.relativePath == "src/main.cpp") {
            foundMainCpp = true;
            check(f.bytes && !f.bytes->empty(), "src/main.cpp has content");
        }
        if (f.relativePath == "empty.txt") {
            foundEmptyTxt = true;
            check(f.bytes && f.bytes->empty(), "empty.txt is valid with 0 bytes");
        }
    }
    check(foundMainCpp, "src/main.cpp included in snapshot");
    check(foundEmptyTxt, "empty.txt included in snapshot");
}

// 5. Test loadSnapshot: binary file skipped with partial coverage and diagnostic
void testSnapshotDetectsBinaryFiles(const TestGitRepo& fixture) {
    std::cout << "--- testSnapshotDetectsBinaryFiles ---\n";
    xray::JobContext ctx;
    xray::history::RepositorySpec spec{ fixture.repoPath };
    xray::code::FileSelection sel;
    sel.extensions = { ".bin", ".cpp", ".hpp" };

    auto res = xray::history::loadSnapshot(spec, fixture.binaryCommitOid, sel, ctx);
    check(std::holds_alternative<xray::code::SourceSnapshot>(res), "loadSnapshot succeeded");
    if (!std::holds_alternative<xray::code::SourceSnapshot>(res)) return;

    const auto& snap = std::get<xray::code::SourceSnapshot>(res);
    check(snap.coverage.completeness == xray::Completeness::partial,
          "coverage is partial due to skipped binary file");
    check(snap.coverage.skippedElements == 1, "exactly 1 skipped element");
    check(snap.coverage.diagnostics.size() == 1, "exactly 1 diagnostic recorded");
    if (!snap.coverage.diagnostics.empty()) {
        const auto& d = snap.coverage.diagnostics[0];
        check(d.code == xray::ErrorCode::unsupported_encoding, "diagnostic code is unsupported_encoding");
        check(d.path.has_value() && *d.path == "assets/binary.bin", "diagnostic path matches binary file");
    }

    // Binary file must NOT be present in snapshot files
    bool foundBinary = false;
    for (const auto& f : snap.files) {
        if (f.relativePath == "assets/binary.bin") foundBinary = true;
    }
    check(!foundBinary, "binary file excluded from source snapshot files");
}

// 6. Test invalid repository and invalid OIDs
void testInvalidInputs(const TestGitRepo& fixture) {
    std::cout << "--- testInvalidInputs ---\n";
    xray::JobContext ctx;

    // Bad repo path
    {
        xray::history::RepositorySpec badSpec{ "C:/non/existent/repo/dir" };
        xray::history::HistoryRequest req;
        auto res = xray::history::query(badSpec, req, ctx);
        check(std::holds_alternative<xray::Error>(res), "non-existent repo query returns Error");
        if (std::holds_alternative<xray::Error>(res)) {
            check(std::get<xray::Error>(res).code == xray::ErrorCode::not_found,
                  "non-existent repo error code is not_found");
        }
    }

    // Bad start OID
    {
        xray::history::RepositorySpec spec{ fixture.repoPath };
        xray::history::HistoryRequest req;
        req.startOid = "invalid_hex_oid";
        auto res = xray::history::query(spec, req, ctx);
        check(std::holds_alternative<xray::Error>(res), "invalid startOid returns Error");
    }

    // Bad loadSnapshot OID
    {
        xray::history::RepositorySpec spec{ fixture.repoPath };
        xray::code::FileSelection sel;
        auto res = xray::history::loadSnapshot(spec, "0000000000000000000000000000000000000000", sel, ctx);
        check(std::holds_alternative<xray::Error>(res), "unknown OID loadSnapshot returns Error");
        if (std::holds_alternative<xray::Error>(res)) {
            check(std::get<xray::Error>(res).code == xray::ErrorCode::not_found,
                  "unknown OID error code is not_found");
        }
    }
}

// 7. Test cancellation
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

// 8. Test hasMore flag
void testHasMoreFlag(const TestGitRepo& fixture) {
    std::cout << "--- testHasMoreFlag ---\n";
    xray::JobContext ctx;
    xray::history::RepositorySpec spec{ fixture.repoPath };
    xray::history::HistoryRequest req;
    req.startOid = fixture.mergeCommitOid;
    req.limit = 1;

    auto res = xray::history::query(spec, req, ctx);
    check(std::holds_alternative<xray::history::HistoryResult>(res), "query succeeded");
    if (std::holds_alternative<xray::history::HistoryResult>(res)) {
        const auto& hist = std::get<xray::history::HistoryResult>(res);
        check(hist.commits.size() == 1, "exactly 1 commit returned");
        check(hist.hasMore, "hasMore flag is true");
    }
}

} // namespace

int main() {
    try {
        std::cout << "Initializing TestGitRepo fixture...\n";
        TestGitRepo fixture;
        std::cout << "TestGitRepo created successfully.\n";

        testFirstParentTraversal(fixture);
        testRelativePathFiltering(fixture);
        testParentIndexValidation(fixture);
        testSnapshotTextAndEmptyFile(fixture);
        testSnapshotDetectsBinaryFiles(fixture);
        testInvalidInputs(fixture);
        testCancellation(fixture);
        testHasMoreFlag(fixture);

        std::cout << "\n========================================\n";
        std::cout << "Summary: " << gPass << " passed, " << gFail << " failed.\n";
        std::cout << "========================================\n";
        return (gFail == 0) ? 0 : 1;
    } catch (const std::exception& ex) {
        std::cerr << "Fatal exception during tests: " << ex.what() << "\n";
        return 2;
    }
}
