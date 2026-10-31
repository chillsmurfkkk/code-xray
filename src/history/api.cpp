#include "history/api.hpp"
#include "common/result.hpp"

#include <git2.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
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
    default:                     return std::nullopt;
    }
}

// Validate that a byte sequence is valid UTF-8 according to RFC 3629
bool isValidUtf8(const char* data, std::size_t len) noexcept {
    const auto* p = reinterpret_cast<const unsigned char*>(data);
    const auto* end = p + len;

    while (p < end) {
        unsigned char c = *p++;
        if (c <= 0x7F) {
            // ASCII (disallowing NUL for source code)
            if (c == '\0') return false;
            continue;
        } else if (c >= 0xC2 && c <= 0xDF) {
            // 2-byte sequence
            if (p >= end) return false;
            unsigned char c2 = *p++;
            if ((c2 & 0xC0) != 0x80) return false;
        } else if (c >= 0xE0 && c <= 0xEF) {
            // 3-byte sequence
            if (p + 1 >= end) return false;
            unsigned char c2 = *p++;
            unsigned char c3 = *p++;
            if ((c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80) return false;
            if (c == 0xE0 && c2 < 0xA0) return false; // overlong
            if (c == 0xED && c2 >= 0xA0) return false; // surrogate 0xD800..0xDFFF
        } else if (c >= 0xF0 && c <= 0xF4) {
            // 4-byte sequence
            if (p + 2 >= end) return false;
            unsigned char c2 = *p++;
            unsigned char c3 = *p++;
            unsigned char c4 = *p++;
            if ((c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80 || (c4 & 0xC0) != 0x80) return false;
            if (c == 0xF0 && c2 < 0x90) return false; // overlong
            if (c == 0xF4 && c2 > 0x8F) return false; // > 0x10FFFF
        } else {
            return false;
        }
    }
    return true;
}

// Check whether a path passes the FileSelection filter (including nested directory exclusions)
bool pathMatchesSelection(const std::string& relPath,
                          const code::FileSelection& sel) {
    std::filesystem::path p(relPath);

    if (!sel.relativePaths.empty()) {
        bool found = false;
        for (const auto& rp : sel.relativePaths) {
            if (relPath == rp || relPath.starts_with(rp + "/") || relPath.starts_with(rp + "\\")) {
                found = true; break;
            }
        }
        if (!found) return false;
    }

    // Check every component in directory path against excludedDirectories
    for (const auto& component : p.parent_path()) {
        const std::string dirName = component.string();
        for (const auto& excl : sel.excludedDirectories) {
            if (dirName == excl) return false;
        }
    }

    if (!sel.extensions.empty()) {
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
                    const std::optional<std::string>& exactPath,
                    std::vector<ChangeRecord>& out) {
    const std::size_t numDeltas = git_diff_num_deltas(diff);
    out.reserve(out.size() + numDeltas);

    for (std::size_t i = 0; i < numDeltas; ++i) {
        const git_diff_delta* delta = git_diff_get_delta(diff, i);
        if (!delta) continue;

        auto maybeStatus = mapDeltaStatus(delta->status);
        if (!maybeStatus) continue;

        const char* oldP = delta->old_file.path;
        const char* newP = delta->new_file.path;

        if (exactPath.has_value()) {
            bool matches = (newP && *exactPath == newP) || (oldP && *exactPath == oldP);
            if (!matches) continue;
        }

        ChangeRecord rec;
        rec.commitOid = commitOid;
        rec.parentOid = parentOid;
        rec.status    = *maybeStatus;
        rec.isBinary  = (delta->flags & GIT_DIFF_FLAG_BINARY) != 0;

        if (oldP && oldP[0]) rec.oldPath = oldP;
        if (newP && newP[0]) rec.newPath = newP;

        PatchHandle patch;
        if (git_patch_from_diff(&patch.ptr, diff, i) == 0 && patch) {
            std::size_t added = 0, deleted = 0, ctx = 0;
            git_patch_line_stats(&ctx, &added, &deleted, patch.get());
            rec.addedLines   = added;
            rec.deletedLines = deleted;
        }
        out.push_back(std::move(rec));
    }
}

// Open a repository at the given path (bare, non-bare, or discovered)
RepoHandle openRepository(const std::filesystem::path& path) {
    RepoHandle repo;
    const auto pathStr = path.string();
    if (git_repository_open(&repo.ptr, pathStr.c_str()) != 0) {
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
// query()  — first-parent commit traversal, collect CommitRecord + ChangeRecord
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

    RevwalkHandle walker;
    if (git_revwalk_new(&walker.ptr, repo.get()) != 0)
        return makeGitError("git_revwalk_new failed");

    // E0 requirement: history traversal must strictly be first-parent.
    git_revwalk_sorting(walker.get(), GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME);
    git_revwalk_simplify_first_parent(walker.get());

    if (!request.startOid.empty()) {
        git_oid startGitOid{};
        if (git_oid_fromstrp(&startGitOid, request.startOid.c_str()) != 0) {
            return xray::Error{ xray::ErrorCode::invalid_input,
                                "Invalid start OID: " + request.startOid };
        }
        if (git_revwalk_push(walker.get(), &startGitOid) != 0)
            return makeGitError("git_revwalk_push failed for OID: " + request.startOid);
    } else {
        if (git_revwalk_push_head(walker.get()) != 0)
            return makeGitError("git_revwalk_push_head failed (repository may be empty)");
    }

    HistoryResult result;
    result.commits.reserve(std::min(request.limit + 1, std::size_t{64}));

    std::size_t seen = 0;
    git_oid oid{};
    int walkRc = 0;

    // Diff options setup with literal pathspec to prevent fnmatch wildcard interpretation
    std::string filterPath;
    const char* pathSpec[1] = { nullptr };
    git_diff_options diffOpts = GIT_DIFF_OPTIONS_INIT;
    diffOpts.flags = GIT_DIFF_INCLUDE_TYPECHANGE;

    if (request.relativePath.has_value()) {
        filterPath = *request.relativePath;
        pathSpec[0] = filterPath.c_str();
        diffOpts.pathspec.strings = const_cast<char**>(pathSpec);
        diffOpts.pathspec.count   = 1;
        // Literal pathspec: do not treat '*', '?', '[' as wildcards
        diffOpts.flags |= GIT_DIFF_DISABLE_PATHSPEC_MATCH;
    }

    while ((walkRc = git_revwalk_next(&oid, walker.get())) == 0) {
        if (ctx.isCancelled()) return xray::Cancelled{};

        CommitHandle commit;
        if (git_commit_lookup(&commit.ptr, repo.get(), &oid) != 0) {
            return makeGitError("Failed to lookup commit " + oidToString(oid),
                                xray::ErrorCode::read_error);
        }

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
            while (!rec.message.empty() && (rec.message.back() == '\n' || rec.message.back() == '\r'))
                rec.message.pop_back();
        }

        // --- Select parent for diff ---
        TreeHandle parentTree;
        std::optional<std::string> parentOidStr;

        if (parentCount == 0) {
            // Root commit: if explicitly queried as startOid with parentIndex > 0, reject
            if (!request.startOid.empty() && request.startOid == rec.oid && request.parentIndex != 0) {
                return xray::Error{
                    xray::ErrorCode::invalid_input,
                    "Root commit has no parents (parentIndex must be 0)",
                    std::nullopt,
                    rec.oid
                };
            }
            // Root commit compares against an empty tree (parentTree remains null)
        } else {
            unsigned int actualParentIndex = 0;
            if (parentCount > 1) {
                // Merge commit: use requested parentIndex (validated against parentCount)
                if (request.parentIndex >= parentCount) {
                    return xray::Error{
                        xray::ErrorCode::invalid_input,
                        "Invalid parentIndex " + std::to_string(request.parentIndex) +
                        " for merge commit " + rec.oid + " (commit has " + std::to_string(parentCount) + " parents)",
                        std::nullopt,
                        rec.oid
                    };
                }
                actualParentIndex = static_cast<unsigned int>(request.parentIndex);
            } else {
                // Non-merge commit: always diff against its single parent (index 0)
                actualParentIndex = 0;
            }

            const git_oid* pOid = git_commit_parent_id(commit.get(), actualParentIndex);
            if (!pOid) {
                return makeGitError("Failed to get parent ID for commit " + rec.oid,
                                    xray::ErrorCode::read_error);
            }
            parentOidStr = oidToString(*pOid);

            CommitHandle parentCommit;
            if (git_commit_lookup(&parentCommit.ptr, repo.get(), pOid) != 0) {
                return makeGitError("Failed to lookup parent commit " + *parentOidStr,
                                    xray::ErrorCode::read_error);
            }
            if (git_commit_tree(&parentTree.ptr, parentCommit.get()) != 0) {
                return makeGitError("Failed to get tree for parent commit " + *parentOidStr,
                                    xray::ErrorCode::read_error);
            }
        }

        // --- Get Commit Tree ---
        TreeHandle commitTree;
        if (git_commit_tree(&commitTree.ptr, commit.get()) != 0) {
            return makeGitError("Failed to get tree for commit " + rec.oid,
                                xray::ErrorCode::read_error);
        }

        // --- Build Diff with Pathspec ---
        DiffHandle diff;
        if (git_diff_tree_to_tree(&diff.ptr, repo.get(),
                                  parentTree.get(), commitTree.get(),
                                  &diffOpts) != 0 || !diff) {
            return makeGitError("git_diff_tree_to_tree failed for commit " + rec.oid,
                                xray::ErrorCode::read_error);
        }

        // Rename detection
        git_diff_find_options findOpts = GIT_DIFF_FIND_OPTIONS_INIT;
        findOpts.flags = GIT_DIFF_FIND_RENAMES;
        git_diff_find_similar(diff.get(), &findOpts);

        // Check if diff contains changes matching exact relativePath
        std::size_t matchingDeltas = 0;
        const std::size_t numDeltas = git_diff_num_deltas(diff.get());
        for (std::size_t i = 0; i < numDeltas; ++i) {
            const git_diff_delta* d = git_diff_get_delta(diff.get(), i);
            if (!d) continue;
            if (request.relativePath.has_value()) {
                const char* oldP = d->old_file.path;
                const char* newP = d->new_file.path;
                if ((newP && filterPath == newP) || (oldP && filterPath == oldP)) {
                    ++matchingDeltas;
                }
            } else {
                ++matchingDeltas;
            }
        }

        // If filtering by relativePath and this commit did not touch it, skip
        if (request.relativePath.has_value() && matchingDeltas == 0) {
            continue;
        }

        // Check limit threshold
        if (seen >= request.limit) {
            if (!request.relativePath.has_value()) {
                result.hasMore = true;
                break;
            }

            // For relativePath queries, look ahead to see if any older commit
            // actually modifies the requested path before setting hasMore = true.
            bool foundMore = true; // current commit already matched and exceeds limit
            result.hasMore = foundMore;
            break;
        }

        collectChanges(diff.get(), rec.oid, parentOidStr,
                       request.relativePath.has_value() ? std::optional<std::string>{filterPath} : std::nullopt,
                       result.changes);

        ctx.report("Reading commits", seen, std::nullopt);
        result.commits.push_back(std::move(rec));
        ++seen;
    }

    // If we filled exactly request.limit commits for relativePath, check if there's any
    // older commit down the revwalk that touches the path to correctly set hasMore.
    if (request.relativePath.has_value() && seen == request.limit && !result.hasMore && walkRc == 0) {
        git_oid lookaheadOid{};
        while (git_revwalk_next(&lookaheadOid, walker.get()) == 0) {
            if (ctx.isCancelled()) return xray::Cancelled{};
            CommitHandle laCommit;
            if (git_commit_lookup(&laCommit.ptr, repo.get(), &lookaheadOid) != 0) continue;
            TreeHandle laCommitTree;
            if (git_commit_tree(&laCommitTree.ptr, laCommit.get()) != 0) continue;

            TreeHandle laParentTree;
            const unsigned int laPCount = git_commit_parentcount(laCommit.get());
            if (laPCount > 0) {
                const git_oid* laPOid = git_commit_parent_id(laCommit.get(), 0);
                CommitHandle laPCommit;
                if (laPOid && git_commit_lookup(&laPCommit.ptr, repo.get(), laPOid) == 0) {
                    git_commit_tree(&laParentTree.ptr, laPCommit.get());
                }
            }

            DiffHandle laDiff;
            if (git_diff_tree_to_tree(&laDiff.ptr, repo.get(),
                                      laParentTree.get(), laCommitTree.get(),
                                      &diffOpts) == 0 && laDiff) {
                git_diff_find_options fOpts = GIT_DIFF_FIND_OPTIONS_INIT;
                fOpts.flags = GIT_DIFF_FIND_RENAMES;
                git_diff_find_similar(laDiff.get(), &fOpts);

                const std::size_t n = git_diff_num_deltas(laDiff.get());
                for (std::size_t i = 0; i < n; ++i) {
                    const git_diff_delta* d = git_diff_get_delta(laDiff.get(), i);
                    if (!d) continue;
                    const char* oldP = d->old_file.path;
                    const char* newP = d->new_file.path;
                    if ((newP && filterPath == newP) || (oldP && filterPath == oldP)) {
                        result.hasMore = true;
                        break;
                    }
                }
            }
            if (result.hasMore) break;
        }
    }

    if (walkRc != 0 && walkRc != GIT_ITEROVER) {
        return makeGitError("Revwalk traversal failed", xray::ErrorCode::read_error);
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
        return makeGitError("Failed to get commit tree for: " + oid, xray::ErrorCode::read_error);

    code::SourceSnapshot snapshot;
    snapshot.kind      = code::SourceKind::git;
    snapshot.commitOid = oid;
    snapshot.selection = selection;
    snapshot.id        = oid;

    struct WalkState {
        git_repository*             repo;
        const code::FileSelection&  sel;
        code::SourceSnapshot&       snapshot;
        const JobContext&           ctx;
        std::size_t                 skippedElements = 0;
        std::vector<xray::Error>    diagnostics;
        bool                        cancelled = false;
    };

    WalkState state{ repo.get(), selection, snapshot, ctx };

    const int walkRes = git_tree_walk(
        tree.get(),
        GIT_TREEWALK_PRE,
        [](const char* root, const git_tree_entry* entry, void* payload) -> int {
            auto& s = *static_cast<WalkState*>(payload);
            if (s.ctx.isCancelled()) { s.cancelled = true; return -1; }

            const char* name = git_tree_entry_name(entry);
            if (!name) return 0;

            const std::string relPath = std::string(root ? root : "") + name;

            // 1. If path does not match selection filters (including nested excluded directories),
            // silently skip. Normal selection filtering does NOT count as a skipped error.
            if (!pathMatchesSelection(relPath, s.sel)) {
                return 0;
            }

            // 2. Path matched selection: inspect entry mode and type.
            const git_filemode_t mode = git_tree_entry_filemode(entry);

            // Submodules (GIT_FILEMODE_COMMIT)
            if (mode == GIT_FILEMODE_COMMIT) {
                xray::Error diag;
                diag.code = xray::ErrorCode::invalid_input;
                diag.path = relPath;
                diag.message = "Submodule entry skipped: " + relPath;
                s.diagnostics.push_back(std::move(diag));
                ++s.skippedElements;
                return 0;
            }

            // Symbolic links (GIT_FILEMODE_LINK)
            if (mode == GIT_FILEMODE_LINK) {
                xray::Error diag;
                diag.code = xray::ErrorCode::invalid_input;
                diag.path = relPath;
                diag.message = "Symbolic link skipped: " + relPath;
                s.diagnostics.push_back(std::move(diag));
                ++s.skippedElements;
                return 0;
            }

            if (git_tree_entry_type(entry) != GIT_OBJECT_BLOB) {
                return 0;
            }

            // Lookup blob
            BlobHandle blob;
            if (git_blob_lookup(&blob.ptr, s.repo, git_tree_entry_id(entry)) != 0) {
                xray::Error diag;
                diag.code = xray::ErrorCode::read_error;
                diag.path = relPath;
                diag.message = "Failed to lookup blob for file: " + relPath;
                s.diagnostics.push_back(std::move(diag));
                ++s.skippedElements;
                return 0;
            }

            // Check if binary
            if (git_blob_is_binary(blob.get()) == 1) {
                xray::Error diag;
                diag.code = xray::ErrorCode::unsupported_encoding;
                diag.path = relPath;
                diag.message = "Binary file skipped: " + relPath;
                s.diagnostics.push_back(std::move(diag));
                ++s.skippedElements;
                return 0;
            }

            const char* rawData = static_cast<const char*>(git_blob_rawcontent(blob.get()));
            const git_object_size_t rawSize = git_blob_rawsize(blob.get());

            // Validate UTF-8 encoding (reject non-UTF8 such as Windows-1251 or corrupt bytes)
            if (rawData && rawSize > 0) {
                if (!isValidUtf8(rawData, static_cast<std::size_t>(rawSize))) {
                    xray::Error diag;
                    diag.code = xray::ErrorCode::unsupported_encoding;
                    diag.path = relPath;
                    diag.message = "Unsupported encoding (non-UTF-8) skipped: " + relPath;
                    s.diagnostics.push_back(std::move(diag));
                    ++s.skippedElements;
                    return 0;
                }
            }

            // Empty files (rawSize == 0) are valid text files with 0 bytes.
            auto bytes = std::make_shared<std::string>(
                (rawData && rawSize > 0) ? std::string(rawData, static_cast<std::size_t>(rawSize)) : std::string()
            );

            code::SourceFile sf;
            sf.relativePath = relPath;
            sf.bytes        = std::move(bytes);

            char oidStr[GIT_OID_SHA1_HEXSIZE + 1] = {};
            git_oid_tostr(oidStr, sizeof(oidStr), git_tree_entry_id(entry));
            sf.contentId = oidStr;

            s.snapshot.files.push_back(std::move(sf));
            return 0;
        },
        &state
    );

    if (state.cancelled) return xray::Cancelled{};

    // Check if git_tree_walk encountered an error
    if (walkRes != 0) {
        return makeGitError("git_tree_walk failed for commit: " + oid, xray::ErrorCode::read_error);
    }

    if (state.skippedElements > 0) {
        snapshot.coverage.completeness    = xray::Completeness::partial;
        snapshot.coverage.skippedElements = state.skippedElements;
        snapshot.coverage.diagnostics     = std::move(state.diagnostics);
    } else {
        snapshot.coverage.completeness    = xray::Completeness::complete;
        snapshot.coverage.skippedElements = 0;
    }

    return snapshot;
}

} // namespace xray::history
