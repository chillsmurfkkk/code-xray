# Analysis profiles and rules

The analysis module provides three polymorphic rules through `Rule&`:
`LongFunctionRule` (`function_lines > 60`), `DeepNestingRule` (`max_nesting > 3`),
and `ManyBranchesRule` (`branch_count > 10`). These are configurable defaults.
Equality is not a violation. Each rule checks its enabled flag, metric kind,
entity ID, body availability and validity before producing a `Finding`.
Unknown values are skipped; `syntactic_only` values retain a warning.
Explanations include the function, file/line, observed value, threshold and metric unit.

## Profile schema v1

See [default-v1.json](../testdata/profiles/default-v1.json). All fields are required:

```json
{
  "schemaVersion": 1,
  "id": "default-v1",
  "name": "Default",
  "rules": {
    "long_function": {"enabled": true, "threshold": 60},
    "deep_nesting": {"enabled": true, "threshold": 3},
    "many_branches": {"enabled": true, "threshold": 10}
  }
}
```

Only schema version 1 is accepted. Thresholds are integers from 0 to INT64_MAX,
including zero. Floats, booleans, numeric strings, negatives and overflowing
unsigned integers are rejected. Enabled flags must be JSON booleans. Unknown
fields/rules, missing fields and duplicate keys are rejected. ID and name must
be nonblank valid UTF-8 without ASCII control characters. Serialization orders
object keys consistently and preserves every profile field.

## API and integration

Include `analysis/profiles.hpp`; no JSON type crosses the public boundary.

- `validateProfile` returns an optional structured error.
- `decodeProfile` / `encodeProfile` exchange JSON text and typed profiles.
- `loadProfile` returns a new profile or a contextual error. Assign the current
  profile only after checking the result; failed imports cannot mutate it.
- `saveProfile` validates first, writes and closes a temporary file in an
  exclusively created directory beside the destination, then replaces the
  destination. Failed writes/replacements preserve the previous file; temporary
  files are cleaned up. POSIX uses rename; Windows uses MoveFileExW with
  REPLACE_EXISTING / WRITE_THROUGH. This does not promise recovery from power loss.
- `duplicateProfile` preserves settings and supplies a new generated ID and name.
- `isAnalysisCurrent(snapshot, profile)` compares **profile settings only**.
  Changing any threshold or enabled flag makes the old analysis stale; changing
  name/ID does not. Invalid profiles return false. This helper neither mutates
  nor reruns analysis. The future UI/coordinator must call it after profile edits
  to display stale results. Grammar/source compatibility remains the separate
  `analysisFingerprint` contract.

`analyze()`, the complete analysis fingerprint, reducers and GUI integration
remain subsequent work. This PR provides the profile/rule layer they will use.

## Build and verification

JSON 3.12.0 is linked privately to `xray_analysis`; vcpkg already pins it.
`CODE_XRAY_BUILD_PROFILES=ON` is the default and enables `profile_tests`.
The Windows `contracts-only` preset explicitly disables profiles to preserve
its dependency-free purpose. Enable profiles when checking this feature:

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
ctest --preset windows-x64
```

On macOS with nlohmann/json 3.12.0 installed:

```sh
cmake --preset macos-contracts
cmake --build --preset macos-contracts
ctest --preset macos-contracts
```

The macOS source-provider build now guards the MSVC-specific `file_type::junction`
checks with `_MSVC_STL_VERSION`. The `macos-contracts` CMake build succeeds and
all eight CTest suites pass: review, compare, trend, history_test, contracts,
analysis, profiles and source_provider. The profiles public header also compiles
independently. Windows, Qt startup and Tree-sitter parser execution have not been
verified in this environment.

`tests/analysis/profile_tests.cpp` covers all three rules through `Rule&`,
threshold equality/excess, toggles, wrong/missing/unavailable metrics, precision
warnings, JSON round trips and invalid input, duplicate IDs for copied profiles,
stale analysis after edits, UTF-8 paths, replacement, failed imports/saves and
cleanup of temporary files. Existing `tests/analysis_tests.cpp` remains green.
