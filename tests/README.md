# Tests

**Owner:** WS-11. Brief: [RB-14](../docs/research/RB-14-testing-automation.md).

`AGENTS.md` rule 7: every deliverable ships with tests. Rule 6: success requires
verification. **Neither is possible until this harness exists**, which is why RB-14 is a
Wave 1 blocker for every other workstream.

## How to run (WS-11 harness)

```bash
# Fast, no editor — run this always
python tests/run_unit_tests.py

# Shipping path (UEREMCP enabled; needs Protocol+Validation DLLs):
pwsh tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter "UEREMCP.Validation"

# Interim probe launch-smoke only (not the shipping Rollback gate — C-3):
pwsh tests/run_editor_tests.ps1 -Filter "UEREMCP.ValidationProbe.Launch.Smoke"

# Engine FileSandbox lifecycle (Epic):
pwsh tests/run_editor_tests.ps1 -Filter "AI.ToolsetRegistry.Sandbox.Library" -NoProbe

# Transport constraints and current JobRegistry skips:
pwsh tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter "UEREMCP.Transport"
```

**SoT for Rollback:** `Plugins/UEREMCP/Source/UeremcpValidation/.../RollbackMultiAssetDiscard.spec.cpp`  
Raw logs gitignored; redacted notes may live under `tests/integration/_logs/*.redacted.md`.  
Scratch: `/Game/__UeremcpTests/` + `FUeremcpScratchGuard` — see RB-14.

The shipping Validation gate is green (6/6) with UEREMCP enabled
`[VERIFIED-RUNTIME: editor_UEREMCP_Validation_20260730_005518.log]`. This permits
`rollback.available: true` only for the tested Content/full-`Discard()` cases below.
Engine probe evidence alone is not enough, and the runtime result must not be broadened
to `Saved/`, `Config/`, `DiscardFiles()`, or untested asset/reference topologies.

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

### The tests that gate architectural claims

Named in the ADRs. Until each passes at runtime, the corresponding claim is not made:

| Automation path | Gates | ADR | Status (2026-07-30) |
|---|---|---|---|
| `UEREMCP.Validation.Rollback.MultiAssetDiscard` | Content package-add rollback | ADR-0005 q1/q3 | **PASS** |
| `UEREMCP.Validation.Rollback.DeletedAssetDiscard` | One pre-existing `UCurveFloat` delete restored by full Discard | ADR-0005 q5 | **PASS (scoped)** |
| `UEREMCP.Validation.Rollback.BlueprintCompileDiscard` | One trivial Actor Blueprint member edit, compile/save, disk/reload restoration | ADR-0005 q4 | **PASS (scoped)** |
| `UEREMCP.Validation.Idempotency.RepeatedCreate` | In-process create replay through `TryGetReplay` | ADR-0006 | **PASS** |
| `UEREMCP.Validation.Revision.StaleRejected` | `expected_revision` reject with no mutation | ADR-0006 | **PASS** |

All five rows plus `UEREMCP.Validation.Harness.Smoke` passed in one shipping-plugin run
`[VERIFIED-RUNTIME: editor_UEREMCP_Validation_20260730_005518.log]`; see
`integration/_logs/editor_UEREMCP_Validation_20260730_6of6.redacted.md`.

ADR-0005 q4/q5 are closed only for those fixtures. Remaining rollback residuals:

- q4: arbitrary Blueprint graph/bytecode/CDO state and dependent loaded objects were
  not exercised; the fixture proves restored package bytes and a clean reload.
- q5: complex assets, referencers, redirectors, and multi-package deletion sets were
  not exercised; the fixture proves one pre-existing curve is restored on disk and in
  the asset registry.
- `Saved/` and `Config/` are outside FileSandbox coverage
  `[VERIFIED: ISandboxInstance.h:28-30]`.
- Partial rollback through `DiscardFiles()` remains unproven and does not perform the
  full purge/hot-reload path `[VERIFIED: SandboxLibrary.cpp:178-203]`.

Full filter:

```powershell
pwsh tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter "UEREMCP"
```

Plus, per family, the ADR-0004 round-trip test: retrieve → replace unchanged → retrieve,
asserting an identical `content_hash`. **A family that cannot pass it does not claim
round-trip support** — it sets `fidelity.round_trip_supported: false` and lists
`lossy_areas`.

Upcoming Niagara and Material runtime filters have a no-claim handoff checklist in
[`integration/Domain.Runtime.Handoffs.md`](integration/Domain.Runtime.Handoffs.md).

### Transport JobRegistry unskip gate

The last recorded shipping Transport run was **5 PASS + 3 SKIP**, although Unreal
Automation reported all eight paths as `Success`
`[VERIFIED-RUNTIME: editor_UEREMCP_Transport_20260730_010212.log; recorded in
docs/research/RB-04-transport-and-jobs.md]`. The skipped paths are Poll, Cancel, and
Timeout.PartiallyCompleted; do not report them as passes.

The deterministic fixtures and exact conversion criteria are in
[`integration/Transport.JobRegistry.Unskip.md`](integration/Transport.JobRegistry.Unskip.md).
They remain documentation-only until WS-05 lands callable JobRegistry symbols.

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

### Optional: Protocol golden vectors (WS-05)

```powershell
pwsh tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter "UEREMCP.Protocol.Golden"
```

2026-07-30: Protocol goldens **pass** (`UEREMCP.Protocol.Golden`). Not a C-3 gate; WS-05 owns parity.
