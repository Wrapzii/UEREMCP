# WS-01 residual plan — A6 and overall POC-B

- **Orch tip at writing:** `40cb2a5` (WS-07 MCP B1/B10 handoffs landed; residual commit follows)
- **Date:** 2026-07-30
- **Status:** **Overall POC A CLAIMED** — CRT on tip `3756244` passes A1–A11 (`tests/integration/_logs/poc_a_complete_round_trip_3756244.json`; 3 MCP calls, 4 internal ops, 2.30s, no errors). Scope caveat: demonstrated simple-graph / native `EventBeginPlay→Branch→PrintString` slice with honest A10 lossy_areas. Post-UV editor fireball and B8 restart proofs pass. WS-07's script-state-only MCP compile await landed as `d07f8f1` (`4dc53b7`) and Niagara rebuilt cleanly with no editor running; WS-11 must rerun B1/B6 and prove one round trip. B10 and complete metrics remain blocked — **no overall POC-B claim.**

Sources: `docs/POC_ACCEPTANCE.md`, `docs/WORK_ALLOCATION.md`, `docs/proposals/ws-01-editor-filter-results.md`.

---

## What A6 requires (exactly)

From `docs/POC_ACCEPTANCE.md` POC A criterion **A6**:

> Re-read the graph and confirm the expected nodes exist and the expected connections are present — asserted programmatically, not by eye.

A6 is **one** criterion inside POC A (A1–A11). It presupposes the prior steps in the same scenario:

| Prerequisite | Criterion |
|---|---|
| Retrieve complete graph in one MCP call | A1–A2 |
| Diagnostics for dead node + disconnected subgraph | A3 |
| External JSON modify (branch + function call, exec wired) | A4 |
| Submit `mode: replace` in one MCP call; compile + save | A5 |
| **Then** programmatic re-read of expected nodes/links | **A6** |
| Status `modified_and_validated` + `validation.reread_after_write: true` | A7 |
| Unchanged replace → identical `content_hash` | A8 |
| Whole scenario ≤ 3 MCP calls | A9 |
| Honest `fidelity.lossy_areas` | A10 |
| Second identical replace → `no_change_required`, no recompile | A11 |

**Owner:** WS-06 (`Plugins/UEREMCP/Source/UeremcpBlueprint/**`). WS-11 runs harness / editor proof. WS-01 owns acceptance criteria text only.

### What Wave 2 Blueprint evidence does **not** satisfy

Recorded proof: `UeremcpBlueprint.Toolset` **PASS 4/4** on tip `35b4cab` (`docs/proposals/ws-01-editor-filter-results.md`).

| Recorded test | What it proves | Why it is not A6 / POC A |
|---|---|---|
| `ReadGraphRoundTrip` | Retrieve + `content_hash`/`revision` alignment | No A4 external modify + A5 replace + A6 programmatic node/link assert |
| `SubmitGraphValidation` | Unchanged replace → `no_change_required`; stale revision rejected; dry-run / validation paths | Not the full A4→A6 modify-then-reread assertion; not A7 `modified_and_validated` + `reread_after_write: true` as the POC A scenario |

Those Wave 2 tests alone do not prove A6. The later dedicated `PocA6Reread` PASS on `c87b1db` does; it still does not prove overall POC A.

### POC A A1–A11 slice (WS-11, tip lineage `d1eb1ea`→`279f09a`)

| Criterion | Result | Note |
|---|---|---|
| A1 | **SKIP** | One MCP call not proven |
| A2 | **SKIP** | Complete payload fields not all asserted |
| A3 | **PASS** | |
| A4 | **PASS** | |
| A5 | **SKIP** | One MCP call not proven |
| A6 | **PASS** | Also dedicated `PocA6Reread` PASS on `c87b1db` |
| A7 | **PASS** | |
| A8 | **PASS** | |
| A9 | **SKIP** | No MCP round-trip metrics |
| A10 | **SKIP** | `lossy_areas` not asserted |
| A11 | **PASS** | |

At that slice, the aggregate filter was missing. A later transport run is recorded below; overall POC A remains unclaimed.

### CompleteRoundTrip transport run (`600c383` lineage)

| Criterion | Result | Evidence |
|---|---|---|
| A1 | **PASS** | One complete read call |
| A2 | **PASS** | Complete payload assertions |
| A5 | **FAIL** | Changed submit returned `failed_validation`: Epic `BlueprintTools` toolset not found |
| A9 | **FAIL** | Only 2/3 planned MCP calls completed |
| A10 | **PASS** | Honest `fidelity.lossy_areas` asserted |

Result: `tests/integration/_logs/poc_a_complete_round_trip_600c383_retry.json`. Downstream changed-submit criteria were not completed in this aggregate run; prior dedicated A6/A8/A11 proofs remain separate. Selector-key correction landed as `eccc282` (`69654dd`). **No overall POC-A claim.**

### CompleteRoundTrip re-run on tip `70cc348` (WS-11; pre-`ee905ed`)

| Criterion | Result | Note |
|---|---|---|
| A1 | **PASS** | |
| A2 | **PASS** | |
| A3 | **PASS** | |
| A4 | **PASS** | |
| A5 | **FAIL** | Python/MCP conflict on SubmitGraph path |
| A9 | **FAIL** | Blocked by A5 |
| A10 | **PASS** | |

BlueprintTools bootstrap `809f863` did not clear A5. Python-free SubmitGraph landed as `d7fe3b2` (`e58466f`).

### CompleteRoundTrip on tip `3756244` (WS-11; Python-free SubmitGraph)

| Criterion | Result | Evidence |
|---|---|---|
| A1–A11 | **ALL PASS** | Aggregate `outcome: pass` |
| Metrics | 3 MCP calls; 4 internal ops; 2.30s wall; 0 errors | Same evidence file |

Evidence: `tests/integration/_logs/poc_a_complete_round_trip_3756244.json`.

### Overall POC A evaluation (`docs/POC_ACCEPTANCE.md`)

**Claim: overall POC A met** at the demonstrated simple-graph CRT scenario.

| # | Required by acceptance | Evidence on `3756244` |
|---|---|---|
| A1 | One MCP complete graph retrieve | PASS |
| A2 | Reconstructable payload | PASS |
| A3 | Dead node + disconnected subgraph diagnostics | PASS |
| A4 | External JSON modify (branch + function call) | PASS (`BeginPlay→Branch→PrintString`) |
| A5 | One-call `replace`; compile + save | PASS (native Python-free writer) |
| A6 | Programmatic re-read of expected nodes/links | PASS |
| A7 | `modified_and_validated` + `reread_after_write: true` | PASS |
| A8 | Unchanged replace → identical `content_hash` | PASS |
| A9 | ≤ 3 MCP calls | PASS (exactly 3) |
| A10 | Honest `fidelity.lossy_areas` | PASS (lists MultiGate/Timeline/bind/etc.) |
| A11 | Second identical replace → `no_change_required`, no recompile | PASS |

**Scope caveats (not gaps that block the claim):** writer is the bounded native semantic slice, not every Epic Blueprint DSL expression; `fidelity.round_trip_supported: false` is honest A10 documentation. POC_ACCEPTANCE already allows simple-graph POC A when A10 documents the boundary. **Not claimed:** arbitrary complex Blueprint graphs beyond this scenario; POC B; E7 metrics file.

### Closed A6 / POC A evidence trail

| Item | Owner | Status |
|---|---|---|
| A6 code / harness | WS-06 | Landed earlier; CRT A6 PASS on `3756244` |
| Python-free SubmitGraph | WS-06 | `d7fe3b2` (`e58466f`) |
| Aggregate CRT A1–A11 | WS-11 | **PASS** — overall POC A claimed |
| Metrics file (`poc-metrics.md`) | WS-11 / WS-14 | Still open for POC **E7** only — not a POC A blocker |

**WS-01 next step:** one-request MCP fireball B1, required B10 visible-render
proof, and complete POC-B metrics/baseline; keep overall POC-B unclaimed.

---

## What overall POC-B still lacks beyond B7 / Wave 2 filters

POC B owner: **WS-07**, depends on **WS-08** for materials (`docs/POC_ACCEPTANCE.md`, `docs/WORK_ALLOCATION.md`).

Recorded Wave 2 evidence that is **in scope but insufficient for overall POC-B**:

| Evidence | Tip | Scope |
|---|---|---|
| `UEREMCP.Niagara.Create` PASS 10/10 | `2384112` | Create suite, not B1–B10 |
| `UEREMCP.Niagara.Inspect` PASS 4/4 | `2384112` | Inspect suite |
| `UEREMCP.Niagara.POCB.SixEmitterGateScaffold` (B7) PASS 1/1 | `825e4f4` (`7394f49` lineage) | **B7 structural scaffold only** (`proof=editor_create_reread_honesty`) |
| Niagara change-manifest + extended `poc_b_gates` (B3/B5/B6/B9) | `6b92576` (`5e3566d`) | **Scaffolding / offline tests only** |
| Fireball material sub-manifest merge, `reuse_if_present`, six-role defaults, B2 gate scaffolding | `2814283` (`51583af`) | **B2/B4 wiring ready** — not live fireball proof |
| Material `reused_assets` B2 slice + WS-08 handoff; claimed Material filter 12/12 on WS-08 branch | `b1f9479` (`150f61a`) | Material-side B2 slice; **not** overall POC-B / B2 fireball proof |
| Strict POC evidence harness (`run_poc_acceptance.ps1`, `poc_evidence.py`) | `761e7fa` (`d0913f8`) | Evidence tooling only — does not itself satisfy A6/POC-B |
| Fireball inline-material editor filter + runner + unit harness | `fcff5cc` (`11efc23`) | Scaffold landed; **prior proof SKIP** on path rejection |
| Niagara `__UeremcpPoc` path allowlist | `76a5f5a` (`004264f`) | Unblocks Niagara create/inspect under POC root — **not** fireball PASS |
| Material `__UeremcpPoc` path allowlist | `c7b33cc` (`a26090f`) | Unblocks material writes under POC root — **not** fireball PASS |
| Blueprint A6 reread-after-write implementation | `13bf529` (`2a0b2cd`) | **Code landed; runtime-unproven** |
| WS-11 runtime follow-up | `d691316` | Fireball **FAIL**; A6 **FAIL**; Material **PASS 14/14** |
| Fireball proof parsing fix | `674c439` (`656ffe0`) | Harness/log parsing fix only; no acceptance claim |
| Material system-path API | `58036dd` (`81b56e2`) | Provides `ExecuteCreateVfxMaterialForNiagaraSystem` before Niagara call sites |
| Inline Niagara material co-location fix | `dc4f118` (`60cb3a4`) | Uses system-path resolve/execute APIs; **B4 not yet re-proven** |
| Direct sprite/ribbon B4 binding fix | `72241c2` (`4e82c68`) | Replaces mismatched `SetRendererData` JSON path with direct bind + UObject re-read; **not yet re-proven** |
| Trail graph depth-fade/panning fix | `a567a3a` (`4f17911`) | Material-side trail master graph fix; awaiting fireball re-run |
| Honest master-only partial failure | `a73bef7` (`6b1b4a0`) | Fails honestly when primary MI is absent |
| FlowMap defaults + all-requested B4 honesty | `13412dd` (`799fc94`) | Injects trail FlowMap and requires every requested role; B2/B4 editor gate PASS on `a6ca454` |
| B1 / B8-save / B9 / `emitter_added` gate scaffolding | `0e79641` (`501aff6`) | Surfaces gates + B8 restart handoff; **not** overall POC-B; WS-11 must assert/restart |
| B8 Restart.Create/Verify filters | `163b272` (`5c6422f`) | Filters + handoff fixture landed; stable log parsing `931f41a` landed; restart proof still required |
| Expanded fireball B-gate assertions | `3b39dd3` (`0474e4e`) | Filter asserts B1/B3/B5/B6/B9 fields; runtime proof pending; ribbon_trail regression under diagnosis |
| Stale ProjTrail master verify/rebuild | `7a417bb` (`dbb3638`) | Ribbon-trail regression fix landed; Material rebuilt; fireball runtime re-run required |
| Stale ProjTrail pre-create cleanup | `886d09d` (`ee905ed`) | Clears stale masters when ribbon MI is absent; defense in depth with `7a417bb`; Niagara rebuilt |
| Trail MainTexture UV + master package release | `cf7e6d3` (`2187d69`) | WS-08 UV pin fix; Material rebuilt; post-UV fireball and B8 later passed |
| Ribbon_trail proposal closed | `5ec7e02` (`ff648ab`) | Documents WS-08 `2187d69` as the durable trail fix; not a POC-B claim |
| MCP B1 fixture + B10 honesty/handoffs | `40cb2a5` (`07f81f5`) | Canonical `poc_b_mcp_fireball_request.json`; `B10_visible_render: null`; WS-11 B1/B10 work remains |
| `UeremcpMaterial.Toolset` PASS 11/11 + live VisualTest MCP T1a | `7535e6c` | Disk-save / validate:false honesty |

### POC-root allowlist status

Prior blocker (`docs/proposals/ws-11-pocb-poc-root-blocker.md`): Niagara/Material rejected `/Game/__UeremcpPoc/`.

**Runtime result on `c87b1db`:** all six MIs were created under `/Game/__UeremcpPoc/Materials/`, and B2 manifest assertions passed. B4 remained false: only `flame_shell` binding verified; `core`, `sparks`, `smoke`, `ribbon_trail`, and `impact_burst` failed re-read verification. Log: `editor_UEREMCP_Niagara_POCB_FireballInlineMaterials_20260730_052723.log`.

**Runtime result on `279f09a`:** direct binding improved verification to 5/6 roles, but `ribbon_trail` failed. No `/Game/__UeremcpPoc/Materials/MI_NS_POCB_Fireball_ribbon_trail` was created (only its master existed). `B4_material_bindings_verified: true` and `validation.material_bindings_verified: true` were dishonest because they covered only five resolved roles; the gate must require every requested role. Log: `editor_UEREMCP_Niagara_POCB_FireballInlineMaterials_20260730_053740.log`.

**Runtime result on `a6ca454`:** **PASS** — all six requested material roles, including `ribbon_trail`, passed B4 binding re-read; B2/B4 editor gate outcome was `PASS`. Log: `tests/integration/_logs/editor_UEREMCP_Niagara_POCB_FireballInlineMaterials_20260730_055332.log`. This is B2/B4 editor evidence only, not overall POC-B.

**Runtime result on `70cc348` (pre-`ee905ed`; has `dbb3638`→`7a417bb`):** fireball **FAIL** — B1/B4 FAIL on `ribbon_trail`; B3/B5/B6/B8_save/B9 **PASS**. B8 Create **FAIL** (same MI / ribbon path). Stacked cleanup `886d09d` (`ee905ed`) landed after this run. **No overall POC-B claim.**

**Runtime result on fresh post-`886d09d` DLLs:** fireball **FAIL** — `ribbon_trail` remains absent/broken after both stale-master defenses. Evidence: `tests/integration/_logs/editor_UEREMCP_Niagara_POCB_FireballInlineMaterials_20260730_063745.log`. B8 was **SKIPPED**. WS-08 `FireballRibbonTrailPoc` / trail graph is the critical blocker. **No overall POC-B claim.**

**Runtime result after loading WS-08 `2187d69` (orch `cf7e6d3`):** editor
`FireballInlineMaterials` **PASS** with all six MIs, including `ribbon_trail`; the
isolated `FireballRibbonTrailPoc` also passed. Evidence:
`editor_UEREMCP_Niagara_POCB_FireballInlineMaterials_20260730_064451.log`.
The expanded editor filter reports B1/B3/B5/B6/`B8_assets_saved`/B9 PASS and
an aggregate PASS. This closes the trail UV/editor-create blocker and provides
current B2/B4 evidence. It does **not** prove acceptance B1's one-MCP-request
transport requirement: the test directly executes one editor create pipeline.
Its B8 field proves only package save; it does **not** prove restart survival.

**WS-11 freshness on tip `8a8c75d`:** `FireballInlineMaterials` **PASS** for
the six requested roles, including `ribbon_trail`, with editor gate fields
B1–B6 and B9 green. Evidence:
`editor_UEREMCP_Niagara_POCB_FireballInlineMaterials_20260730_064554.log`.
For acceptance, this is current B2–B6/B9 proof; its B1 field still describes
the direct editor single-create pipeline, not one MCP transport request.

**B8 restart proof on tip `8a8c75d`: PASS.** WS-11 ran separate Create and
Verify editor processes. Verify reports `restart_observed` and
`reread_after_restart` for all ten checkpoint assets. Evidence:
`editor_UEREMCP_Niagara_POCB_Restart_Create_20260730_064653.log` and
`editor_UEREMCP_Niagara_POCB_Restart_Verify_20260730_064733.log`.

**WS-11 MCP B1 attempt on orch `3b69e8f`: FAIL.** The canonical fixture reached
`CreateNiagaraEffect` with six materials and validation enabled, then the editor
crashed in `UeremcpNiagaraCreate.cpp:589` during `AwaitCompile`. The editor
`FireballInlineMaterials` proof remains PASS; it does not supersede this MCP
transport failure. WS-07 owns the crash fix. B10 and POC-B metrics are blocked
behind a successful MCP create. **No overall POC-B claim.**

**Crash fix landed:** WS-07 `132bb54` is orch `088bd64`. `AwaitCompile` now
uses poll-only completion and avoids reentrant game-thread draining. The
`UeremcpNiagara` module rebuilt successfully. This is fix readiness only:
WS-11 must rerun `poc_b_mcp_fireball_request.json` before B1 can pass.

**Post-fix MCP rerun on orch `8322ee6`: FAIL.** With `088bd64` loaded, the
canonical WS-11 B1 run still crashes on a SharedPointer `IsValid` assertion at
`UeremcpNiagaraCreate.cpp:593` during `AwaitCompile`. The editor fireball
filter remains PASS. WS-07 is investigating the remaining MCP-only crash.

**Second crash fix landed:** WS-07 `9c9b9b4` is orch `79d9d65`. Live MCP
dispatch now requests compilation but does not call compile-completion polling;
automation retains its poll path. `UeremcpNiagara` rebuilt successfully.
WS-11 must rerun the canonical fixture and confirm a JSON response without an
editor crash. An honest `partially_completed` response is acceptable crash-fix
evidence but does not by itself satisfy B1/B6 or overall POC B.

**Post-`79d9d65` MCP rerun: crash fixed, acceptance incomplete.** WS-11
received a JSON response without an editor crash:

- `status: partially_completed`
- `metrics.mcp_round_trips: 1`
- `B1_single_request_complete: false`
- `B6_compile_awaited: false`
- `checks_skipped`: `niagara.compile_await_deferred_tool_dispatch`

This proves crash safety and one transport round trip, but not a complete
one-request effect or genuinely awaited compilation. The remaining implementation
choice is a safe compile-complete path that does not use the crashing query, or
ADR-0009 job completion. A polled job can complete safely, but its follow-up MCP
poll calls do not satisfy B1's explicit “no follow-up calls” wording.

**Script-state await fix landed:** WS-07 `4dc53b7` is orch `d07f8f1`. MCP
compile await now observes `GetSystemCompileState`
`[VERIFIED: Plugins/UEREMCP/Source/UeremcpNiagara/Private/UeremcpNiagaraCompileAwait.cpp:77-111]`
without the crash-inducing query path; Niagara rebuilt cleanly after confirming
no editor was running. WS-11 must rerun the canonical fixture and prove
`B1_single_request_complete: true`, `B6_compile_awaited: true`, and
`metrics.mcp_round_trips: 1`. The fix/build alone is not a POC-B claim.

### POC B checklist (B1–B10)

| # | Criterion (short) | Status vs current evidence | Owner |
|---|---|---|---|
| B1 | One MCP request produces the complete effect — no follow-ups | **OPEN after `d07f8f1`** — prior run failed; WS-11 must prove B1 true in one MCP round trip | WS-07 + WS-11 |
| B2 | Materials created or reused; reuse in `result.reused_assets` | **PASS on `8a8c75d` editor fireball** — all six MI roles represented | WS-08 + WS-07 |
| B3 | Niagara system exists with all six requested emitters | **PASS on `8a8c75d` editor fireball** | WS-07 + WS-11 |
| B4 | Renderers configured and bound to valid materials | **PASS on `8a8c75d` editor fireball** — all six, including `ribbon_trail` | WS-07 + WS-08 |
| B5 | User params for colour, scale, intensity | **PASS on `8a8c75d` editor fireball** | WS-07 + WS-11 |
| B6 | System compiles; compile genuinely awaited | **OPEN after `d07f8f1`** — editor gate passes; MCP script-state await requires runtime proof | WS-07 + WS-11 |
| B7 | Structural validation: emitters non-empty, renderers bound, no missing DIs | **PASS scaffold** on `825e4f4` only — not overall POC-B | WS-07 |
| B8 | Assets saved and survive editor restart | **PASS on `8a8c75d`** — Create→restart→Verify re-read all ten checkpoint assets | WS-11 |
| B9 | One structured response with complete change manifest | **PASS on `8a8c75d` editor fireball** | WS-07 + WS-11 |
| B10 | Visibly renders as fireball when placed — screenshot supplementary only | **OPEN and required** — it is a numbered binary acceptance criterion; screenshot alone cannot satisfy it | WS-07 / WS-11 |

Global POC rules still apply: real RE project; scratch under `/Game/__UeremcpPoc/`; metrics reported numerically; success with only `validate:false` does **not** count (`docs/POC_ACCEPTANCE.md`).

---

## Follow-ups (do not launch false claims)

| Priority | Follow-up | Owner | WS-01 action |
|---|---|---|---|
| P0 | Rerun canonical MCP fixture with script-state await | WS-11 | Expect B1/B6 true and exactly one MCP round trip; no claim until observed |
| P0 | If using ADR-0009 job completion, preserve honest round-trip accounting | WS-07 + WS-05/WS-11 | Job polling may solve completion, but follow-up polls do not satisfy current B1 wording |
| P0 | Place and visibly render the fireball with non-screenshot validation | WS-11 | Blocked by MCP B1; B10 remains honestly `null` |
| P0 | Record POC-B MCP/internal-operation/token/wall metrics and equivalent primitive-call count | WS-11 / WS-14 | Blocked by MCP B1; required before overall claim |
| P1 | Record measured metrics in `docs/reviews/poc-metrics.md` (E7) | WS-11 / WS-14 | POC A numbers exist in CRT evidence; empty metrics file is not a claim |
| P2 | POC C / D / E remainder | WS-07+15 / WS-09 / WS-11 | Out of this residual’s critical path after POC A + POC B |

**Claimed:** overall POC A (CRT A1–A11 on tip `3756244`, simple-graph / native slice scope).

**Not claimed:** overall POC B, or POC-A beyond the demonstrated CRT scenario.

**Junction:** RE and VisualTest must remain on `UEREMCP-ws01\Plugins\UEREMCP`.
