#include "history/api.hpp"
#include "common/result.hpp"

#include <git2.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace xray::history {

// ---------------------------------------------------------------------------
// RAII helpers
// ---------------------------------------------------------------------------
namespace {

struct LibGit2Guard {
    LibGit2Guard()  { git_libgit2_init(); }
    ~LibGit2Guard() { git_libgit2_shutdown(); }
    LibGit2Guard(const LibGit2Guard&) = delete;
    LibGit2Guard& operator=(const LibGit2Guard&) = delete;
};

template<class T, void(*Deleter)(T*)>
struct GitHandle {
    T* ptr = nullptr;
    explicit GitHandle(T* p = nullptr) noexcept : ptr(p) {}
    ~GitHandle() { if (ptr) Deleter(ptr); }
    GitHandle(const GitHandle&) = delete;
    GitHandle& operator=(const GitHandle&) = delete;
    GitHandle(GitHandle&& o) noexcept : ptr(o.ptr) { o.ptr = nullptr; }
    GitHandle& operator=(GitHandle&& o) noexcept {
        if (&o != this) { if (ptr) Deleter(ptr); ptr = o.ptr; o.ptr = nullptr; }
        return *this;
    }
    T* get()       const noexcept { return ptr; }
    explicit operator bool() const noexcept { return ptr != nullptr; }
};

using RepoHandle    = GitHandle<git_repository, git_repository_free>;
using CommitHandle  = GitHandle<git_commit,     git_commit_free>;
using TreeHandle    = GitHandle<git_tree,        git_tree_free>;
using DiffHandle    = GitHandle<git_diff,        git_diff_free>;
using PatchHandle   = GitHandle<git_patch,       git_patch_free>;
using BlobHandle    = GitHandle<git_blob,        git_blob_free>;
using RevwalkHandle = GitHandle<git_revwalk,     git_revwalk_free>;
using ObjectHandle  = GitHandle<git_object,      git_object_free>;

// Translate libgit2 error into xray::Error
xray::Error makeGitError(const std::string& msg, xray::ErrorCode code = xray::ErrorCode::read_error) {
    xray::Error err;
    err.code = code;
    err.message = msg;
    const git_error* raw = git_error_last();
    if (raw && raw->message) {
        err.message += std::string(": ") + raw->message;
    }
    return err;
}

// Convert git_oid to hex string
std::string oidToString(const git_oid& oid) {
    char buf[GIT_OID_SHA1_HEXSIZE + 1] = {};
    git_oid_tostr(buf, sizeof(buf), &oid);
    return buf;
}

// Convert git_time to sys_seconds
std::chrono::sys_seconds gitTimeToSysSecs(const git_time& t) {
    return std::chrono::sys_seconds{ std::chrono::seconds{ t.time } };
}

// Map libgit2 delta status to our ChangeStatus
std::optional<ChangeStatus> mapDeltaStatus(git_delta_t delta) {
    switch (delta) {
    case GIT_DELTA_ADDED:        return ChangeStatus::added;
    case GIT_DELTA_DELETED:      return ChangeStatus::deleted;
    case GIT_DELTA_MODIFIED:     return ChangeStatus::modified;
    case GIT_DELTA_RENAMED:      return ChangeStatus::renamed;
    case GIT_DELTA_TYPECHANGE:   return ChangeStatus::type_changed;
    // Untracked, ignored, conflicted, copied -> skip
    default:                     return std::nullopt;
    }
}

// Check whether a path passes the FileSelection filter
bool pathMatchesSelection(const std::string& relPath,
                          const code::FileSelection& sel) {
    if (!sel.relativePaths.empty()) {
        bool found = false;
        for (const auto& rp : sel.relativePaths) {
            if (relPath == rp || relPath.starts_with(rp + "/") || relPath.starts_with(rp + "\\")) {
                found = true; break;
            }
        }
        if (!found) return false;
    }

    for (const auto& excl : sel.excludedDirectories) {
        if (relPath.starts_with(excl + "/") || relPath.starts_with(excl + "\\"))
            return false;
    }

    if (!sel.extensions.empty()) {
        std::filesystem::path p(relPath);
        const auto ext = p.extension().string();
        bool matched = false;
        for (const auto& e : sel.extensions) {
            if (ext == e) { matched = true; break; }
        }
        if (!matched) return false;
    }
    return true;
}

// Build ChangeRecord list from a diff
void collectChanges(git_diff* diff,
                    const std::string& commitOid,
                    const std::optional<std::string>& parentOid,
                    std::vector<ChangeRecord>& out) {
    const std::size_t numDeltas = git_diff_num_deltas(diff);
    out.reserve(out.size() + numDeltas);

    for (std::size_t i = 0; i < numDeltas; ++i) {
        const git_diff_delta* delta = git_diff_get_delta(diff, i);
        if (!delta) continue;

        auto maybeStatus = mapDeltaStatus(delta->status);
        if (!maybeStatus) continue;

        ChangeRecord rec;
        rec.commitOid = commitOid;
        rec.parentOid = parentOid;
        rec.status    = *maybeStatus;
        rec.isBinary  = (delta->flags & GIT_DIFF_FLAG_BINARY) != 0;

        const char* oldP = delta->old_file.path;
        const char* newP = delta->new_file.path;

        if (oldP && oldP[0]) rec.oldPath = oldP;
        if (newP && newP[0]) rec.newPath = newP;

        // Gather line stats via patch
        PatchHandle patch;
        if (git_patch_from_diff(&patch.ptr, diff, i) == 0 && patch) {
            std::size_t added = 0, deleted = 0, ctx = 0;
            git_patch_line_stats(&ctx, &added, &deleted, patch.get());
            rec.addedLines   = added;
            rec.deletedLines = deleted;
        }
        // If patch failed (binary etc.) lines stay nullopt -> hasUnknownLineCounts
        out.push_back(std::move(rec));
    }
}

// Open a repository at the given path (bare or non-bare, or a worktree)
RepoHandle openRepository(const std::filesystem::path& path) {
    RepoHandle repo;
    const auto pathStr = path.string();
    if (git_repository_open(&repo.ptr, pathStr.c_str()) != 0) {
        // Try to discover the repo root if the path is a work tree subdirectory
        git_buf discovered = GIT_BUF_INIT;
        if (git_repository_discover(&discovered, pathStr.c_str(), 1, nullptr) == 0) {
            git_repository_open(&repo.ptr, discovered.ptr);
        }
        git_buf_dispose(&discovered);
    }
    return repo;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// query()  — walk commits, collect CommitRecord + ChangeRecord lists
// ---------------------------------------------------------------------------
Result<HistoryResult> query(const RepositorySpec& spec,
                            const HistoryRequest& request,
                            const JobContext& ctx)
{
    LibGit2Guard guard;

    RepoHandle repo = openRepository(spec.path);
    if (!repo) {
        return makeGitError("Failed to open repository at " + spec.path.string(),
                            xray::ErrorCode::not_found);
    }

    if (ctx.isCancelled()) return xray::Cancelled{};

    // Set up revwalk
    RevwalkHandle walker;
    if (git_revwalk_new(&walker.ptr, repo.get()) != 0)
        return makeGitError("git_revwalk_new failed");

    git_revwalk_sorting(walker.get(), GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME);

    if (!request.startOid.empty()) {
        git_oid startGitOid{};
        if (git_oid_fromstrp(&startGitOid, request.startOid.c_str()) != 0) {
            return xray::Error{ xray::ErrorCode::invalid_input,
                                "Invalid start OID: " + request.startOid };
        }
        if (git_revwalk_push(walker.get(), &startGitOid) != 0)
            return makeGitError("git_revwalk_push failed for OID: " + request.startOid);
    } else {
        // Push HEAD
        if (git_revwalk_push_head(walker.get()) != 0)
            return makeGitError("git_revwalk_push_head failed (repository may be empty)");
    }

    HistoryResult result;
    result.commits.reserve(std::min(request.limit + 1, std::size_t{64}));

    std::size_t seen = 0;
    git_oid oid{};

    while (git_revwalk_next(&oid, walker.get()) == 0) {
        if (ctx.isCancelled()) return xray::Cancelled{};

        if (seen >= request.limit) {
            result.hasMore = true;
            break;
        }

        CommitHandle commit;
        if (git_commit_lookup(&commit.ptr, repo.get(), &oid) != 0) continue;

        // --- Build CommitRecord ---
        CommitRecord rec;
        rec.oid = oidToString(oid);

        const unsigned int parentCount = git_commit_parentcount(commit.get());
        rec.parentOids.reserve(parentCount);
        for (unsigned int p = 0; p < parentCount; ++p) {
            const git_oid* pOid = git_commit_parent_id(commit.get(), p);
            if (pOid) rec.parentOids.push_back(oidToString(*pOid));
        }

        if (const git_signature* author = git_commit_author(commit.get())) {
            rec.authorName  = author->name  ? author->name  : "";
            rec.authorEmail = author->email ? author->email : "";
            rec.authoredAt  = gitTimeToSysSecs(author->when);
        }
        if (const git_signature* committer = git_commit_committer(commit.get())) {
            rec.committedAt = gitTimeToSysSecs(committer->when);
        }
        if (const char* msg = git_commit_message(commit.get())) {
            rec.message = msg;
            // Trim trailing newlines
            while (!rec.message.empty() && (rec.message.back() == '\n' || rec.message.back() == '\r'))
                rec.message.pop_back();
        }

        // --- Build ChangeRecords via diff ---
        {
            TreeHandle commitTree;
            if (git_commit_tree(&commitTree.ptr, commit.get()) != 0) {
                ++seen;
                result.commits.push_back(std::move(rec));
                continue;
            }

            // Choose which parent to diff against (controlled by request.parentIndex)
            std::optional<std::string> parentOidStr;
            TreeHandle parentTree;

            if (parentCount > 0) {
                const std::size_t pi = std::min(static_cast<std::size_t>(request.parentIndex),
                                                static_cast<std::size_t>(parentCount - 1));
                const git_oid* pOid = git_commit_parent_id(commit.get(), static_cast<unsigned>(pi));
                if (pOid) {
                    parentOidStr = oidToString(*pOid);
                    CommitHandle parentCommit;
                    if (git_commit_lookup(&parentCommit.ptr, repo.get(), pOid) == 0)
                        git_commit_tree(&parentTree.ptr, parentCommit.get());
                }
            }

            // Apply optional path filter
            git_diff_options diffOpts = GIT_DIFF_OPTIONS_INIT;
            diffOpts.flags = GIT_DIFF_INCLUDE_TYPECHANGE;

            std::string filterPath;
            if (request.relativePath.has_value()) {
                filterPath = *request.relativePath;
                const char* pathSpec[] = { filterPath.c_str() };
                diffOpts.pathspec.strings = const_cast<char**>(pathSpec);
                diffOpts.pathspec.count   = 1;
            }

            DiffHandle diff;
            if (git_diff_tree_to_tree(&diff.ptr, repo.get(),
                                      parentTree.get(), commitTree.get(),
                                      &diffOpts) == 0 && diff) {
                // Enable rename detection
                git_diff_find_options findOpts = GIT_DIFF_FIND_OPTIONS_INIT;
                findOpts.flags = GIT_DIFF_FIND_RENAMES;
                git_diff_find_similar(diff.get(), &findOpts);

                collectChanges(diff.get(), rec.oid, parentOidStr, result.changes);
            }
        }

        ctx.report("Reading commits", seen, std::nullopt);
        result.commits.push_back(std::move(rec));
        ++seen;
    }

    return result;
}

// ---------------------------------------------------------------------------
// loadSnapshot() — build code::SourceSnapshot from a specific commit tree
// ---------------------------------------------------------------------------
Result<code::SourceSnapshot> loadSnapshot(const RepositorySpec& spec,
                                          const Oid& oid,
                                          const code::FileSelection& selection,
                                          const JobContext& ctx)
{
    LibGit2Guard guard;

    RepoHandle repo = openRepository(spec.path);
    if (!repo) {
        return makeGitError("Failed to open repository at " + spec.path.string(),
                            xray::ErrorCode::not_found);
    }

    if (ctx.isCancelled()) return xray::Cancelled{};

    git_oid gitOid{};
    if (git_oid_fromstrp(&gitOid, oid.c_str()) != 0) {
        return xray::Error{ xray::ErrorCode::invalid_input,
                            "Invalid OID for loadSnapshot: " + oid };
    }

    CommitHandle commit;
    if (git_commit_lookup(&commit.ptr, repo.get(), &gitOid) != 0)
        return makeGitError("Commit not found: " + oid, xray::ErrorCode::not_found);

    TreeHandle tree;
    if (git_commit_tree(&tree.ptr, commit.get()) != 0)
        return makeGitError("Failed to get commit tree for: " + oid);

    code::SourceSnapshot snapshot;
    snapshot.kind      = code::SourceKind::git;
    snapshot.commitOid = oid;
    snapshot.selection = selection;
    snapshot.id        = oid;

    xray::Coverage coverage;
    std::size_t skipped = 0;

    // Walk tree entries recursively
    struct WalkState {
        git_repository*          repo;
        const code::FileSelection& sel;
        code::SourceSnapshot&    snapshot;
        const JobContext&        ctx;
        std::size_t&             skipped;
        bool                     cancelled = false;
    };

    WalkState state{ repo.get(), selection, snapshot, ctx, skipped };

    git_tree_walk(
        tree.get(),
        GIT_TREEWALK_PRE,
        [](const char* root, const git_tree_entry* entry, void* payload) -> int {
            auto& s = *static_cast<WalkState*>(payload);
            if (s.ctx.isCancelled()) { s.cancelled = true; return -1; }

            if (git_tree_entry_type(entry) != GIT_OBJECT_BLOB) return 0;

            const char* name = git_tree_entry_name(entry);
            if (!name) return 0;

            const std::string relPath = std::string(root ? root : "") + name;
            if (!pathMatchesSelection(relPath, s.sel)) { ++s.skipped; return 0; }

            BlobHandle blob;
            if (git_blob_lookup(&blob.ptr, s.repo, git_tree_entry_id(entry)) != 0) {
                ++s.skipped; return 0;
            }

            const char* rawData = static_cast<const char*>(git_blob_rawcontent(blob.get()));
            const git_object_size_t rawSize = git_blob_rawsize(blob.get());
            if (!rawData || rawSize == 0) { ++s.skipped; return 0; }

            auto bytes = std::make_shared<std::string>(rawData, static_cast<std::size_t>(rawSize));

            code::SourceFile sf;
            sf.relativePath = relPath;
            sf.bytes        = std::move(bytes);
            // Use OID of the blob as a stable content ID
            char oidStr[GIT_OID_SHA1_HEXSIZE + 1] = {};
            git_oid_tostr(oidStr, sizeof(oidStr), git_tree_entry_id(entry));
            sf.contentId = oidStr;

            s.snapshot.files.push_back(std::move(sf));
            return 0;
        },
        &state
    );

    if (state.cancelled) return xray::Cancelled{};

    if (skipped > 0) {
        coverage.completeness    = xray::Completeness::partial;
        coverage.skippedElements = skipped;
    }
    snapshot.coverage = coverage;

    return snapshot;
}

} // namespace xray::history
