# B — evidence for Lab 1

PR4 completes the planned core analysis symbols B01–B25. This checklist records
code and demonstrations, not a promise of acceptance by the instructor.

## Types and fields

The module contains 10 behavior classes: `Metric`, `LengthMetric`, `NestingMetric`,
`BranchCountMetric`, `Rule`, `LongFunctionRule`, `DeepNestingRule`,
`ManyBranchesRule`, `MeanReducer`, `MaxReducer`. These include two inheritance
hierarchies, each with one abstract base and three concrete classes.

Three additional result/profile types owned by B are the shared E0 contracts:
`AnalysisProfile`, `MetricResult`, `Finding`. Their initial declarations predate
PR1–PR4; their origin must remain explicit when discussing individual authorship.
Their fields are used and validated by the implemented module:

| Type | Fields | Count |
|---|---|---:|
| AnalysisProfile | id, schemaVersion, name, longFunctionLimit, nestingLimit, branchLimit, enabledRules | 7 |
| MetricResult | entityId, metricId, value, validity, reason | 5 |
| Finding | id, ruleId, entityId, observedValue, threshold, validity, explanation | 7 |
| MeanReducer | mean_, count_ (private) | 2 |
| MaxReducer | maximum_, entities_ (private) | 2 |

Total in these five types: 23 fields, including 19 shared E0 contract fields.
Inherited fields are not counted again. The instructor/team should confirm the
attribution of shared contract design; do not count the same field for two authors.

## Nontrivial implementation inventory

The following 33 implemented algorithms/helpers provide a conservative inventory.
The three forwarding rule methods are recorded in B01–B25 but excluded from this
count. Constructors, destructors, id/name/target getters, result finish methods,
test helpers and simple error constructors are also excluded.

| File | Implementations | Count |
|---|---|---:|
| metrics.cpp | isNestingConstruct, computeMaxNesting, computeBranchCount, LengthMetric::calculate, NestingMetric::calculate, BranchCountMetric::calculate | 6 |
| rules.cpp | formatFindingExplanation, evaluateThreshold | 2 |
| profiles.cpp | validLabel, requireKeys, nonnegativeInteger, uniqueId, validateProfile, decodeProfile, encodeProfile, loadProfile, saveProfile, duplicateProfile, isAnalysisCurrent | 11 |
| engine.cpp | functions, functionIndex, buildMetricRegistry, resolveEnabledRules, makeAnalysisFingerprint, summarizeCoverage, analyze, filterFindings, groupFindingsByFile, rankFunctions, validateAnalysisResult | 11 |
| aggregation.hpp | MeanReducer::accumulate, MaxReducer::accumulate, aggregateMetrics | 3 |

## Three independent polymorphic uses

1. Dynamic: `analyze()` invokes all three `calculate()` implementations through
   `const Metric&` from a registry of `unique_ptr<Metric>`.
2. Dynamic: `analyze()` invokes enabled `evaluate()` implementations through
   `const Rule&` from a registry of `unique_ptr<Rule>`.
3. Static: `analysis_demo` and engine tests instantiate the custom
   `aggregateMetrics<Reducer>` algorithm with two different reducer types.

## Demonstration

Build and run the commands in [analysis-engine.md](analysis-engine.md). Explain
why the demo's length is 61 and triggers a rule at 60, and why equality at 60 does
not trigger it. Run the engine tests for disabled rules, incomplete results,
profile edits, cancellation, average/max and integration with comparison/trend.
Run profile tests for valid and damaged JSON and safe save failure.

The GUI forms/controls/event handlers are Lab 2 work and are not counted here.
Personal explanation of the algorithms and agreed authorship remain part of the
Lab 1 defense even after all automated tests pass.
