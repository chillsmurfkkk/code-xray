#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <git2.h>

#include "app/coordinator.hpp"
#include "history/types.hpp"
#include "code/types.hpp"
#include "analysis/types.hpp"
#include "review/types.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using namespace xray::app;
using namespace xray::history;
using namespace xray::review;

namespace {
template <typename T, void (*Free)(T*)>
using Handle = std::unique_ptr<T, decltype(Free)>;

std::string makeCommit(git_repository* repo, const std::string& filename,
                       const std::string& content, const std::string& message,
                       const std::vector<git_commit*>& parents = {}) {
    git_oid blobOid{}, treeOid{}, commitOid{};
    if (git_blob_create_from_buffer(&blobOid, repo, content.data(), content.size()) != 0) {
        throw std::runtime_error("git_blob_create_from_buffer failed");
    }

    git_treebuilder* builderRaw = nullptr;
    if (git_treebuilder_new(&builderRaw, repo, nullptr) != 0) {
        throw std::runtime_error("git_treebuilder_new failed");
    }
    Handle<git_treebuilder, git_treebuilder_free> builder(builderRaw, git_treebuilder_free);

    if (git_treebuilder_insert(nullptr, builder.get(), filename.c_str(), &blobOid, GIT_FILEMODE_BLOB) != 0 ||
        git_treebuilder_write(&treeOid, builder.get()) != 0) {
        throw std::runtime_error("git_treebuilder_write failed");
    }

    git_tree* treeRaw = nullptr;
    if (git_tree_lookup(&treeRaw, repo, &treeOid) != 0) {
        throw std::runtime_error("git_tree_lookup failed");
    }
    Handle<git_tree, git_tree_free> tree(treeRaw, git_tree_free);

    git_signature* sigRaw = nullptr;
    if (git_signature_now(&sigRaw, "Tester", "tester@example.com") != 0) {
        throw std::runtime_error("git_signature_now failed");
    }
    Handle<git_signature, git_signature_free> sig(sigRaw, git_signature_free);

    std::vector<const git_commit*> parentPtrs;
    parentPtrs.reserve(parents.size());
    for (auto* p : parents) parentPtrs.push_back(p);

    if (git_commit_create(&commitOid, repo, "HEAD", sig.get(), sig.get(), "UTF-8",
                          message.c_str(), tree.get(), parentPtrs.size(), parentPtrs.data()) != 0) {
        throw std::runtime_error("git_commit_create failed");
    }

    char hex[GIT_OID_SHA1_HEXSIZE + 1]{};
    git_oid_tostr(hex, sizeof(hex), &commitOid);
    return std::string(hex);
}
} // namespace

class CoordinatorTests : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        git_libgit2_init();
    }

    void cleanupTestCase() {
        git_libgit2_shutdown();
    }

    void testFirstParentValidation_ValidChain() {
        // Build mock commits: c3 -> c2 -> c1 (root)
        CommitRecord c1{"c1", {}, "Author", "a@test.com", {}, {}, "Root"};
        CommitRecord c2{"c2", {"c1"}, "Author", "a@test.com", {}, {}, "Second"};
        CommitRecord c3{"c3", {"c2"}, "Author", "a@test.com", {}, {}, "Third"};

        std::vector<CommitRecord> commits = {c3, c2, c1};
        std::string err;

        // Newest-first chain
        QVERIFY(Coordinator::validateFirstParentChain(commits, {"c3", "c2", "c1"}, &err));
        QVERIFY(err.empty());

        // Oldest-first chain
        QVERIFY(Coordinator::validateFirstParentChain(commits, {"c1", "c2", "c3"}, &err));
        QVERIFY(err.empty());

        // Single commit chain
        QVERIFY(Coordinator::validateFirstParentChain(commits, {"c2"}, &err));
    }

    void testFirstParentValidation_InvalidBranchChain() {
        // Merge commit m1 has parent0=c2, parent1=b1 (feature branch)
        CommitRecord c1{"c1", {}, "Author", "a@test.com", {}, {}, "Root"};
        CommitRecord c2{"c2", {"c1"}, "Author", "a@test.com", {}, {}, "Main"};
        CommitRecord b1{"b1", {"c1"}, "Author", "a@test.com", {}, {}, "Branch"};
        CommitRecord m1{"m1", {"c2", "b1"}, "Author", "a@test.com", {}, {}, "Merge"};

        std::vector<CommitRecord> commits = {m1, c2, b1, c1};
        std::string err;

        // Valid first-parent: m1 -> c2 -> c1
        QVERIFY(Coordinator::validateFirstParentChain(commits, {"m1", "c2", "c1"}, &err));

        // Invalid first-parent: trying to include b1 in the chain (b1 is second parent, not first parent!)
        err.clear();
        QVERIFY(!Coordinator::validateFirstParentChain(commits, {"m1", "b1"}, &err));
        QVERIFY(!err.empty());

        // Missing commit
        err.clear();
        QVERIFY(!Coordinator::validateFirstParentChain(commits, {"m1", "c999"}, &err));
        QVERIFY(!err.empty());

        // Empty chain
        err.clear();
        QVERIFY(!Coordinator::validateFirstParentChain(commits, {}, &err));
        QVERIFY(!err.empty());
    }

    void testExtractFirstParentChain() {
        CommitRecord c1{"c1", {}, "Author", "a@test.com", {}, {}, "Root"};
        CommitRecord c2{"c2", {"c1"}, "Author", "a@test.com", {}, {}, "Main"};
        CommitRecord b1{"b1", {"c1"}, "Author", "a@test.com", {}, {}, "Branch"};
        CommitRecord m1{"m1", {"c2", "b1"}, "Author", "a@test.com", {}, {}, "Merge"};

        std::vector<CommitRecord> commits = {m1, c2, b1, c1};
        auto chain = Coordinator::extractFirstParentChain(commits, "m1", 10);

        QCOMPARE(chain.size(), 3);
        QCOMPARE(chain[0].oid, std::string("m1"));
        QCOMPARE(chain[1].oid, std::string("c2"));
        QCOMPARE(chain[2].oid, std::string("c1"));
    }

    void testFullGitPipeline() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        git_repository* repoRaw = nullptr;
        QCOMPARE(git_repository_init(&repoRaw, dir.path().toUtf8().constData(), 0), 0);
        Handle<git_repository, git_repository_free> repo(repoRaw, git_repository_free);

        // Commit 1: function f() { return 1; }
        const std::string code1 = "int f() {\n    return 1;\n}\n";
        std::string c1Oid = makeCommit(repo.get(), "main.cpp", code1, "Initial commit");

        git_commit* c1Commit = nullptr;
        git_oid c1GitOid{};
        git_oid_fromstr(&c1GitOid, c1Oid.c_str());
        QCOMPARE(git_commit_lookup(&c1Commit, repo.get(), &c1GitOid), 0);
        Handle<git_commit, git_commit_free> c1Handle(c1Commit, git_commit_free);

        // Commit 2: function f() { return 2; } and g() { return 42; }
        const std::string code2 = "int f() {\n    return 2;\n}\nint g() {\n    return 42;\n}\n";
        std::string c2Oid = makeCommit(repo.get(), "main.cpp", code2, "Add g and update f", {c1Handle.get()});

        Coordinator coordinator;
        coordinator.setRepositoryPath(std::filesystem::path(dir.path().toStdWString()));

        // 1. Open repository & query
        bool repoQueried = false;
        HistoryResult histRes;
        connect(&coordinator, &Coordinator::repositoryQueried, [&](const HistoryResult& res) {
            repoQueried = true;
            histRes = res;
        });

        coordinator.openRepository(std::filesystem::path(dir.path().toStdWString()));
        QTRY_VERIFY_WITH_TIMEOUT(repoQueried, 5000);
        QCOMPARE(histRes.commits.size(), 2);
        QVERIFY(!histRes.changes.empty());

        // 2. Load single commit snapshot (Git -> A)
        bool snapshotLoaded = false;
        std::shared_ptr<const xray::code::ParsedSnapshot> parsedSnap;
        connect(&coordinator, &Coordinator::commitSnapshotLoaded, [&](auto snap) {
            snapshotLoaded = true;
            parsedSnap = snap;
        });

        coordinator.loadCommitSnapshot(c2Oid);
        QTRY_VERIFY_WITH_TIMEOUT(snapshotLoaded, 5000);
        QVERIFY(parsedSnap != nullptr);
        QCOMPARE(parsedSnap->units.size(), 1);

        // 3. Compare two commits: Git (C) -> Code (A) -> Analysis (B) -> Review (D)
        bool compCompleted = false;
        ComparisonReport compRep;
        connect(&coordinator, &Coordinator::comparisonCompleted, [&](const ComparisonReport& rep) {
            compCompleted = true;
            compRep = rep;
        });

        coordinator.compareCommits(c1Oid, c2Oid);
        QTRY_VERIFY_WITH_TIMEOUT(compCompleted, 5000);
        QVERIFY(!compRep.items.empty());

        // 4. Build Trend along first-parent chain: Git (C) -> Code (A) -> Analysis (B) -> Review (D)
        bool trendCompleted = false;
        TrendReport trendRep;
        connect(&coordinator, &Coordinator::trendCompleted, [&](const TrendReport& rep) {
            trendCompleted = true;
            trendRep = rep;
        });

        coordinator.buildTrend(c2Oid, 5);
        QTRY_VERIFY_WITH_TIMEOUT(trendCompleted, 5000);
        QCOMPARE(trendRep.points.size(), 2);
    }

    void testCancellation() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        git_repository* repoRaw = nullptr;
        QCOMPARE(git_repository_init(&repoRaw, dir.path().toUtf8().constData(), 0), 0);
        Handle<git_repository, git_repository_free> repo(repoRaw, git_repository_free);

        const std::string code = "int x() { return 100; }\n";
        std::string c1Oid = makeCommit(repo.get(), "lib.cpp", code, "Init");

        Coordinator coordinator;
        coordinator.setRepositoryPath(std::filesystem::path(dir.path().toStdWString()));

        QSignalSpy completedSpy(&coordinator, &Coordinator::comparisonCompleted);
        QSignalSpy cancelledSpy(&coordinator, &Coordinator::operationCancelled);

        // Trigger and cancel immediately
        coordinator.compareCommits(c1Oid, c1Oid);
        coordinator.cancel();

        // Must gracefully finish without hang or crash
        QTRY_VERIFY_WITH_TIMEOUT(!coordinator.isBusy(), 5000);
        QCOMPARE(completedSpy.count(), 0); // a cancelled job must not report success
        QVERIFY(cancelledSpy.count() >= 1);
    }

    void testSupersededJobDoesNotLeakCancellation() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        git_repository* repoRaw = nullptr;
        QCOMPARE(git_repository_init(&repoRaw, dir.path().toUtf8().constData(), 0), 0);
        Handle<git_repository, git_repository_free> repo(repoRaw, git_repository_free);

        const std::string code = "int x() { return 100; }\n";
        std::string c1Oid = makeCommit(repo.get(), "lib.cpp", code, "Init");

        Coordinator coordinator;
        coordinator.setRepositoryPath(std::filesystem::path(dir.path().toStdWString()));

        QSignalSpy completedSpy(&coordinator, &Coordinator::comparisonCompleted);
        QSignalSpy cancelledSpy(&coordinator, &Coordinator::operationCancelled);

        // Supersede the first job before the event loop can deliver its result.
        coordinator.compareCommits(c1Oid, c1Oid);
        coordinator.compareCommits(c1Oid, c1Oid);

        QTRY_VERIFY_WITH_TIMEOUT(!coordinator.isBusy(), 5000);
        QCOMPARE(completedSpy.count(), 1); // only the current job reports
        QCOMPARE(cancelledSpy.count(), 0); // stale cancellation must not be shown
    }

    void testDestructorWaitsForSupersededWorkers() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        git_repository* repoRaw = nullptr;
        QCOMPARE(git_repository_init(&repoRaw, dir.path().toUtf8().constData(), 0), 0);
        Handle<git_repository, git_repository_free> repo(repoRaw, git_repository_free);

        const std::string code = "int x() { return 100; }\n";
        std::string c1Oid = makeCommit(repo.get(), "lib.cpp", code, "Init");

        {
            Coordinator coordinator;
            coordinator.setRepositoryPath(std::filesystem::path(dir.path().toStdWString()));
            coordinator.compareCommits(c1Oid, c1Oid);
            coordinator.compareCommits(c1Oid, c1Oid);
            coordinator.compareCommits(c1Oid, c1Oid);
            // Destructor runs here while superseded workers may still be alive.
        }
        QVERIFY(true);
    }
};

QTEST_MAIN(CoordinatorTests)
#include "coordinator_tests.moc"
