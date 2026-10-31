# libgit2: integration notes and retrospective (C, PR3)

Author: Oleksandr Kostiuk (kostukoleksandr)  
Role: Developer C (Git & History module)  
Project: CodeXray

This document records the architectural decisions, implementation details, personal experience,
and reference sources for integrating `libgit2` into the CodeXray History module.

---

## 1. Context and Library Choice

The task of Developer C in CodeXray is inspecting Git repositories without requiring an active working
copy checkout, running fast commit graph traversals, computing parent-relative diffs with rename
detection, and extracting source snapshots directly from commit trees.

`libgit2` was chosen as the underlying library because:
- **Zero runtime dependency on the `git` binary**: It links directly into our application as a native C/C++ library, ensuring deterministic behavior across environments without spawning child processes.
- **Plumbing-level granularity**: Unlike high-level Git CLI wrappers, `libgit2` provides direct access to raw Git object databases (ODB), revwalk topologies, tree iterators, blob contents, and diff deltas.
- **Memory and performance efficiency**: Reading blob contents directly by OID avoids filesystem disk I/O for working tree checkout, which is critical for large repository snapshots.

---

## 2. Implemented Features and Public API Mapping

The integration is encapsulated in `src/history/api.cpp` behind the pure E0 contracts in `history/api.hpp`:
1. `query(spec, request, ctx)`:
   - **Repository Opening**: Uses `git_repository_open` with fallback to `git_repository_discover` for subdirectory paths.
   - **First-Parent Revwalk**: Configures `git_revwalk_new`, `GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME`, and strictly enforces `git_revwalk_simplify_first_parent` to guarantee consistent trend lines and avoid polluting linear release branches with merged feature branch commits.
   - **Commit Metadata Extraction**: Extracts author, committer, Unix timestamps, parent OID arrays, and commit messages.
   - **Multi-Commit Parent Selection**: Validates `request.parentIndex` against merge commits (`parentCount > 1`). For regular single-parent commits (`parentCount == 1`) in a multi-commit first-parent traversal, diffs correctly against their only parent (index 0), preventing crashes when `parentIndex > 0` is passed with `limit > 1`.
   - **Tree-to-Tree Diff**: Uses `git_diff_tree_to_tree` between the target commit tree and the selected parent tree (or an empty root tree if `parentCount == 0`).
   - **Rename Detection**: Enables `git_diff_find_similar` with `GIT_DIFF_FIND_RENAMES`.
   - **Line-Level Patch Stats**: Computes added/deleted lines using `git_patch_line_stats` for text files.
   - **Literal Pathspec and Accurate Delta Filtering**: Employs `GIT_DIFF_DISABLE_PATHSPEC_MATCH` so that filenames containing characters like `*`, `?`, or `[` are treated literally instead of being expanded as fnmatch wildcards. Only changes exactly matching `relativePath` are accumulated.
   - **Exact `hasMore` Lookahead**: For file history queries (`relativePath`), `hasMore` is only set to `true` if an older commit down the revwalk chain actually touched the requested file, preventing false positives at the tail of file history.
   - **Strict Parent Validation**: Rejects invalid `parentIndex` values (e.g., non-zero for root commits, or indices exceeding `parentCount - 1` for merge commits) with `xray::ErrorCode::invalid_input`.
   - **Cooperative Cancellation**: Checks `ctx.isCancelled()` before each revwalk step and returns `xray::Cancelled`.
2. `loadSnapshot(spec, oid, selection, ctx)`:
   - **Tree Traversal with Error Checking**: Performs `git_tree_walk(..., GIT_TREEWALK_PRE, ...)`, and strictly checks its return code to return `xray::ErrorCode::read_error` if traversal of any nested tree fails.
   - **Nested Excluded Directories**: Traverses `std::filesystem::path(relPath).parent_path()` to ensure that directory names listed in `selection.excludedDirectories` (e.g. `vendor`) are excluded at any depth (e.g. `src/vendor/vendored.cpp`).
   - **Selective Scope Filtering**: Paths filtered out by `FileSelection` (unmatched extensions, excluded directories) are skipped naturally without marking the snapshot as partial or corrupting coverage metrics.
   - **Non-Regular File Diagnostics**: Inspects `git_tree_entry_filemode`. Symlinks (`GIT_FILEMODE_LINK`) and submodules (`GIT_FILEMODE_COMMIT`) that match the selection path are skipped with explicit diagnostics recorded in `snapshot.coverage.diagnostics` and tracked in `skippedElements`.
   - **Strict UTF-8 Encoding Validation**: In addition to `git_blob_is_binary` and NUL byte checks, implements RFC 3629 / Unicode Standard byte sequence validation (`isValidUtf8`). Non-UTF-8 text files (e.g., Windows-1251, ISO-8859-1) are rejected with `unsupported_encoding` diagnostics and mark `coverage.completeness = Completeness::partial`.
   - **Zero-Byte File Support**: Handles empty text files (0 bytes) as valid `SourceFile` entries with empty buffers, without false-positive error reporting.

---

## 3. Resource Safety (RAII)

`libgit2` uses manual memory management with specialized free functions for every type. To prevent leaks:
- Implemented `GitHandle<T, void(*)(T*)>` template wrapper for move-only lifetime management (`git_repository_free`, `git_commit_free`, `git_tree_free`, `git_diff_free`, `git_patch_free`, `git_blob_free`, `git_revwalk_free`).
- Implemented `LibGit2Guard` for exception-safe and scope-safe initialization via `git_libgit2_init()` and `git_libgit2_shutdown()`.

---

## 4. Challenges and Key Lessons

1. **Pathspec Wildcard Disabling in C APIs**:
   By default, `libgit2` pathspecs use fnmatch matching. If a valid source file contains characters like `[1]`, git matches it against patterns. Using `GIT_DIFF_DISABLE_PATHSPEC_MATCH` instructs libgit2 to interpret pathspecs as exact literal prefixes/paths.
2. **First-Parent Semantics in Multi-Commit Walks**:
   A query requesting `parentIndex = 1` for a merge commit must not blindly pass index 1 to subsequent non-merge commits in the history chain, as they only have a single parent (index 0). Differentiating merge commits (`parentCount > 1`) from linear commits ensured history traversal continues seamlessly without invalid input errors.
3. **Difference Between Filter Skipping and Diagnostic Errors**:
   A key architectural realization was separating intentional scope exclusion (files filtered out by user-requested extensions or nested excluded directories) from operational failures (binary files, symlinks, unreadable blobs, non-UTF8 encodings). Normal filters must produce `Completeness::complete`, whereas problematic entries within scope produce `Completeness::partial` with diagnostic explanations.
4. **Lookahead for Tail-Accurate Pagination (`hasMore`)**:
   Standard pagination simply checks if revwalk has more commits when `seen == limit`. But for path-filtered history, revwalk might have older commits that never touched the path. Checking ahead until either a relevant commit is found or the walk terminates avoids misleading `hasMore = true` flags.
5. **Deterministic Unit Testing with In-Memory / Isolated Fixtures**:
   Relying on the host project's `.git` folder creates brittle tests vulnerable to shallow clones and commit history changes. Creating a programmatic fixture via `TestGitRepo` using `libgit2` (root, branch, merge, binary, non-UTF8, bracket files, nested vendor) created 100% reproducible, fast, and hermetic tests.

---

## 5. Sources Used

- [libgit2 API Documentation](https://libgit2.org/libgit2/):
  - Repository API: `git_repository_open`, `git_repository_discover`
  - Revwalk API: `git_revwalk_new`, `git_revwalk_simplify_first_parent`, `git_revwalk_sorting`
  - Diff API: `git_diff_tree_to_tree`, `git_diff_find_similar`, `git_patch_line_stats`, `GIT_DIFF_DISABLE_PATHSPEC_MATCH`
  - Tree & Blob API: `git_tree_walk`, `git_blob_lookup`, `git_blob_is_binary`, `git_tree_entry_filemode`
- [Pro Git (Scott Chacon, Ben Straub)](https://git-scm.com/book/en/v2):
  - Chapter 10: *Git Internals* (Plumbing and Porcelain, Git Objects, Tree Objects).
  - Appendix B: *Embedding Git in your Applications* (Libgit2 fundamentals).
- [RFC 3629 / Unicode Standard](https://datatracker.ietf.org/doc/html/rfc3629): UTF-8 validation rules, overlong sequence rejection, surrogate code point restrictions.
- [libgit2 by Example / Tutorials](https://libgit2.org/docs/guides/101-samples/): Reference patterns for commit creation, diff traversal, and tree builder usage.
