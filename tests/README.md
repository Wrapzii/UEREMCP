# Tests

**Owner:** WS-11. Brief: [RB-14](../docs/research/RB-14-testing-automation.md).

`AGENTS.md` rule 7: every deliverable ships with tests. Rule 6: success requires
verification. **Neither is possible until this harness exists**, which is why RB-14 is a
Wave 1 blocker for every other workstream.

## Layout

```
tests/
  unit/          Pure logic. Runs OUTSIDE the editor. Fast.
  integration/   Editor automation tests. Touches real assets.
  benchmark/     Round-trip and token measurement vs the ~5:1 baseline.
```

## `unit/` — where most tests should live

Anything with no editor dependency: envelope parse/serialise, schema validation, graph
serialisation and deserialisation, patch application, `$ref` resolution, dependency
topological sort, revision hashing, idempotency-key handling, template matching, error
conversion.

`UeremcpProtocol` deliberately depends on neither `ToolsetRegistry` nor
`ModelContextProtocol` precisely so this layer is testable without launching the editor
(`Plugins/UEREMCP/README.md`).

**Optimise ruthlessly for speed here.** RB-14 q3 exists because a test suite that takes
ten minutes does not get run, and R-14 is the risk that agents skip verification
entirely — which would quietly defeat the project's central premise.

## `integration/` — editor automation tests

Asset creation, modification, compilation, saving, reloading, graph reconstruction,
transaction rollback, plugin restart, engine restart, broken-asset recovery.

Rules:

- Scratch content path only: **`/Game/__UeremcpTests/`**. Never touch real project
  content.
- Cleanup must be guaranteed even on failure. A test that leaves assets behind will
  poison the next run and, worse, the project.
- Compilation must be **awaited**, not assumed. Blueprint, Niagara, and shader
  compilation are async, and R-10 is the flakiness that results from getting this wrong.

### The three tests that gate architectural claims

Named in the ADRs. Until each passes, the corresponding claim is not made:

| Test | Gates | ADR |
|---|---|---|
| `Rollback.MultiAssetDiscard` | `rollback.available` reports `false` until this passes | ADR-0005 |
| `Idempotency.RepeatedCreate` | Any create-or-update idempotency claim | ADR-0006 |
| `Revision.StaleRejected` | Any optimistic-concurrency claim | ADR-0006 |

Plus, per family, the ADR-0004 round-trip test: retrieve → replace unchanged → retrieve,
asserting an identical `content_hash`. **A family that cannot pass it does not claim
round-trip support** — it sets `fidelity.round_trip_supported: false` and lists
`lossy_areas`.

## `benchmark/`

Measures what ADR-0003 makes mandatory: `mcp_round_trips`, `internal_operations`, total
tokens, wall clock, and **completion rate**.

Extend REAgentTools' existing A/B harness (`$RAT/Docs/BENCHMARK_REPORT.md`,
`benchmark_ab_live.json`) rather than starting fresh, so numbers stay comparable to the
~5:1 baseline. A new benchmark that cannot be compared to that baseline answers nothing
(R-17).

Report three numbers per scenario, never one. **Completion rate is the one that matters
most and the one most likely to be quietly omitted** — see `docs/WHY.md`.

## Writing an honest test

A test that passes because it asserted something trivially true is worse than no test:
it manufactures false confidence in exactly the place this project cannot afford it.

Assert the thing that would actually be wrong. For a graph replace, that means asserting
the specific nodes exist and the specific connections are present — not that the call
returned without error.
