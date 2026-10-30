# libgit2: integration notes and retrospective (C, PR3)

This document records the architectural decisions, implementation details, personal experience,
and reference sources for integrating `libgit2` into the CodeXray History module (Developer C, Lab 3).

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
   - **Tree-to-Tree Diff**: Uses `git_diff_tree_to_tree` between the target commit tree and the selected parent tree (or an empty root tree if `parentCount == 0`).
   - **Rename Detection**: Enables `git_diff_find_similar` with `GIT_DIFF_FIND_RENAMES`.
   - **Line-Level Patch Stats**: Computes added/deleted lines using `git_patch_line_stats` for text files.
   - **Pathspec Filtering**: Filters diffs via `diffOpts.pathspec` and drops commits that do not modify the requested `relativePath`, preventing them from polluting the commit history or exhausting the pagination `limit`.
   - **Strict Parent Validation**: Rejects invalid `parentIndex` values (e.g., non-zero for root commits, or indices exceeding `parentCount - 1`) with `xray::ErrorCode::invalid_input`.
   - **Cooperative Cancellation**: Checks `ctx.isCancelled()` before each revwalk step and returns `xray::Cancelled`.
2. `loadSnapshot(spec, oid, selection, ctx)`:
   - **Tree Traversal**: Performs `git_tree_walk(..., GIT_TREEWALK_PRE, ...)` to iterate all entries of the commit tree.
   - **Selective Scope Filtering**: Paths filtered out by `FileSelection` (unmatched extensions, excluded directories) are skipped naturally without marking the snapshot as partial or corrupting coverage metrics.
   - **Non-Regular File Diagnostics**: Inspects `git_tree_entry_filemode`. Symlinks (`GIT_FILEMODE_LINK`) and submodules (`GIT_FILEMODE_COMMIT`) that match the selection path are skipped with explicit diagnostics recorded in `snapshot.coverage.diagnostics` and tracked in `skippedElements`.
   - **Binary and Corrupt File Detection**: Uses `git_blob_is_binary` and inspects embedded NUL bytes. Binary files are excluded from `SourceSnapshot.files` and reported as `unsupported_encoding` in diagnostics, correctly marking `coverage.completeness = Completeness::partial`.
   - **Zero-Byte File Support**: Handles empty text files (0 bytes) as valid `SourceFile` entries with empty buffers, without false-positive error reporting.

---

## 3. Resource Safety (RAII)

`libgit2` uses manual memory management with specialized free functions for every type. To prevent leaks:
- Implemented `GitHandle<T, void(*)(T*)>` template wrapper for move-only lifetime management (`git_repository_free`, `git_commit_free`, `git_tree_free`, `git_diff_free`, `git_patch_free`, `git_blob_free`, `git_revwalk_free`).
- Implemented `LibGit2Guard` for exception-safe and scope-safe initialization via `git_libgit2_init()` and `git_libgit2_shutdown()`.

---

## 4. Challenges and Key Lessons

1. **Scope and Lifetime in C APIs**:
   Passing array pointers such as `diffOpts.pathspec.strings` requires ensuring the backing C strings and pointer arrays live for the duration of the C API call (`git_diff_tree_to_tree`), rather than expiring at the end of conditional inner blocks.
2. **First-Parent Semantics in Merge Commits**:
   Standard revwalk without `git_revwalk_simplify_first_parent` traverses all branches merged into the trunk. For metric tracking and historical trends, traversing secondary parents breaks linear time ordering. Enforcing first-parent simplification solved this.
3. **Difference Between Filter Skipping and Diagnostic Errors**:
   A key architectural realization was separating intentional scope exclusion (files filtered out by user-requested extensions) from operational failures (binary files, symlinks, unreadable blobs). Normal filters must produce `Completeness::complete`, whereas problematic entries within scope produce `Completeness::partial` with diagnostic explanations.
4. **Deterministic Unit Testing with In-Memory / Isolated Fixtures**:
   Relying on the host project's `.git` folder creates brittle tests vulnerable to shallow clones and commit history changes. Creating a programmatic fixture via `TestGitRepo` using `libgit2` (root, branch, merge, binary, empty files) created 100% reproducible, fast, and hermetic tests.

---

## 5. Sources Used

- [libgit2 API Documentation](https://libgit2.org/libgit2/):
  - Repository API: `git_repository_open`, `git_repository_discover`
  - Revwalk API: `git_revwalk_new`, `git_revwalk_simplify_first_parent`, `git_revwalk_sorting`
  - Diff API: `git_diff_tree_to_tree`, `git_diff_find_similar`, `git_patch_line_stats`
  - Tree & Blob API: `git_tree_walk`, `git_blob_lookup`, `git_blob_is_binary`, `git_tree_entry_filemode`
- [Pro Git (Scott Chacon, Ben Straub)](https://git-scm.com/book/en/v2):
  - Chapter 10: *Git Internals* (Plumbing and Porcelain, Git Objects, Tree Objects).
  - Appendix B: *Embedding Git in your Applications* (Libgit2 fundamentals).
- [libgit2 by Example / Tutorials](https://libgit2.org/docs/guides/101-samples/): Reference patterns for commit creation, diff traversal, and tree builder usage.
