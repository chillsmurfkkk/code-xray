# Analysis engine (B, PR4 / Lab 1)

`analyze()` is now implemented. It consumes immutable `ParsedSnapshot` data,
validates the profile and entity IDs, computes all three metrics through `Metric&`,
and applies enabled rules through `Rule&`. Disabling rules never disables metrics.
The result retains its parsed snapshot and a copy of the profile. Profile edits
from a progress callback cannot change a running job.

Pre-cancelled jobs and cancellation at progress boundaries return `Cancelled`,
with inherited diagnostics and no partial `AnalysisSnapshot`. Progress includes
job ID, initial zero, each completed function and total. Empty input is successful.
Parser coverage is inherited once; unavailable functions force partial coverage.
`summarizeCoverage` counts valid, syntactic-only, unavailable and non-applicable
functions separately. It does not add retained functions to skipped file counts.

The profile/engine implementations are enabled by `CODE_XRAY_BUILD_PROFILES`.
The dependency-free `contracts-only` preset checks their public types, but does
not build the JSON-backed profile/engine implementation or engine tests.

## Compatibility fingerprint

`makeAnalysisFingerprint` returns the canonical representation itself, encoded
as byte-length-prefixed UTF-8 fields. It includes analysis/metric/rule versions,
grammar and structure versions, sorted unique selection lists, schema version,
all thresholds and enabled flags. It excludes source IDs/OIDs, content, discovered
files and profile name/ID. It is a compatibility key, not a security hash.
Changes to metric/rule behavior require bumping the corresponding version tag.

The analysis ID combines length-prefixed source ID and fingerprint. Repeating the
same snapshot/settings produces the same ID and result ordering. Different
snapshots can have the same fingerprint and remain comparable.

## Aggregates and presentation helpers

`aggregateMetrics<Reducer>` is a custom template algorithm, instantiated with
`MeanReducer` and `MaxReducer` in both tests and `analysis_demo`. It excludes
unknown/non-applicable values, reports denominators and syntactic-only counts,
and returns `nullopt` for an empty numeric selection. Mean uses a long-double
online accumulator to avoid signed sum overflow. Maximum records every tied
entity in sorted order. Reducer `accumulate` expects a numeric result; the
algorithm checks that precondition before calling it.

`filterFindings` combines rule, path substring, text substring and validity
filters. `groupFindingsByFile` gives file counts via group sizes and stable order.
`rankFunctions` sorts numeric values ascending/descending with stable ties and
unknown values last. These helpers are intended for the future GUI; they do not
modify analysis results. Inputs should come from `analyze()` or pass validation.

`validateAnalysisResult` checks source ownership, IDs, fingerprint, metric schema,
entity references, all three unique metrics per function, validity/value states,
finding uniqueness, enabled rules/thresholds/values, missing findings and inherited
coverage. It validates the structure and consistency, not metric correctness by
running the algorithms a second time.

## Verification

```sh
cmake --preset macos-contracts
cmake --build --preset macos-contracts
ctest --preset macos-contracts
./build/macos-contracts/analysis_demo
```

Expected demo: 3 metrics, 1 finding, mean/max function length 61 and a readable
long-function explanation. This is a hand-built parser-output fixture for an
independent module demonstration; it does not claim to parse real C++ source.

All 9 CTest suites pass locally on AppleClang/C++20 with the Xcode macOS 26.5 SDK.
The new engine tests cover cancellation, invalid/duplicate/null inputs, inherited
partial coverage, profile immutability, compatibility changes, unknown/empty
aggregates, maximum ties, filters/ranking and corrupted results. They also feed
actual `analyze()` output into D's `compare()` and `buildTrend()` implementations.
Windows/Qt/Tree-sitter execution remains unverified here.

GUI forms belong to Lab 2. This PR finishes the planned B01–B25 core symbols;
it does not implement the application's background coordinator or all of F01–F11.
