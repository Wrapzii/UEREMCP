# WS-01 residual plan — A6 and overall POC-B

- **Orch tip at writing:** `931f41a` (WS-11 stable editor-log parsing)
- **Date:** 2026-07-30
- **Status:** Stable `UEREMCP_EDITOR_LOG=` parsing landed as `931f41a` (`ff6c69b`). The stale ProjTrail fix remains at `7a417bb`, and BlueprintTools bootstrap at `809f863`; Material and Blueprint rebuilt successfully. **WS-11 next:** re-run fireball, re-run A5 CRT, and run B8 Create → restart → Verify. No B8 / overall POC-A / overall POC-B claim.

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

### Missing A6 / POC A evidence (owners)

| Gap | Owner | Evidence needed |
|---|---|---|
| **A6 code on orch** | WS-06 | Landed `13bf529` (`2a0b2cd`): programmatic reread-after-write validation + harness handoff `docs/proposals/ws-06-a6-ws11-harness.md`. |
| **A6 selector/no-op fix** | WS-06 | Landed `7b2ed34` (`90b8a6d`): multi-event endpoint IDs + hash-based no-op. |
| Editor/runtime A6 filter PASS | WS-11 (+ WS-06) | **PASS on `c87b1db`** — `editor_UeremcpBlueprint_Toolset_PocA6Reread_20260730_052810.log`; test Success, exit 0 |
| Complete submit evidence scaffolding | WS-06 | Landed `7cd6c93` (`eded4f8`): complete payloads + expanded A1/A2/A5/A8/A10 assertions; proposal `ws-06-a1-a2-a5-ws11-complete-round-trip.md`. **Not a POC-A claim.** |
| A1 / A2 complete read payload | WS-11 | **PASS** in CRT transport run |
| A5 changed submit | WS-06 + WS-11 | Prior **FAIL** — Epic `BlueprintTools` not found; bootstrap fix `809f863` landed, CRT re-run required |
| A9 MCP round-trip metrics | WS-11 | **FAIL** — 2/3 calls completed |
| A10 `fidelity.lossy_areas` | WS-11 | **PASS** in CRT transport run |
| Aggregate `POCA.CompleteRoundTrip` | WS-11 | Prior **FAIL overall**; selector correction `eccc282` and BlueprintTools bootstrap `809f863` landed; re-run required |
| Metrics file for POC A | WS-11 / WS-14 | `docs/reviews/poc-metrics.md` (POC E7) |

**WS-01 next step:** re-run CRT after BlueprintTools bootstrap `809f863`; retain prior A1/A2/A10 slice PASS but refuse current A5/A9 and overall POC-A claims until runtime proof.

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
| `UeremcpMaterial.Toolset` PASS 11/11 + live VisualTest MCP T1a | `7535e6c` | Disk-save / validate:false honesty |

### POC-root allowlist status

Prior blocker (`docs/proposals/ws-11-pocb-poc-root-blocker.md`): Niagara/Material rejected `/Game/__UeremcpPoc/`.

**Runtime result on `c87b1db`:** all six MIs were created under `/Game/__UeremcpPoc/Materials/`, and B2 manifest assertions passed. B4 remained false: only `flame_shell` binding verified; `core`, `sparks`, `smoke`, `ribbon_trail`, and `impact_burst` failed re-read verification. Log: `editor_UEREMCP_Niagara_POCB_FireballInlineMaterials_20260730_052723.log`.

**Runtime result on `279f09a`:** direct binding improved verification to 5/6 roles, but `ribbon_trail` failed. No `/Game/__UeremcpPoc/Materials/MI_NS_POCB_Fireball_ribbon_trail` was created (only its master existed). `B4_material_bindings_verified: true` and `validation.material_bindings_verified: true` were dishonest because they covered only five resolved roles; the gate must require every requested role. Log: `editor_UEREMCP_Niagara_POCB_FireballInlineMaterials_20260730_053740.log`.

**Runtime result on `a6ca454`:** **PASS** — all six requested material roles, including `ribbon_trail`, passed B4 binding re-read; B2/B4 editor gate outcome was `PASS`. Log: `tests/integration/_logs/editor_UEREMCP_Niagara_POCB_FireballInlineMaterials_20260730_055332.log`. This is B2/B4 editor evidence only, not overall POC-B.

### POC B checklist (B1–B10)

| # | Criterion (short) | Status vs current evidence | Owner |
|---|---|---|---|
| B1 | One MCP request produces the complete effect — no follow-ups | **Gate scaffolding landed** at `0e79641`; editor single-create path only until MCP proof | WS-07 + WS-11 |
| B2 | Materials created or reused; reuse in `result.reused_assets` | Prior editor PASS on `a6ca454`; ribbon fix `7a417bb` landed; **fireball re-run required** | WS-08 + WS-07 |
| B3 | Niagara system exists with all six requested emitters | **Assertions expanded** at `3b39dd3`; WS-11 must re-run fireball / B8 create | WS-07 + WS-11 |
| B4 | Renderers configured and bound to valid materials | Prior editor PASS on `a6ca454`; ribbon fix `7a417bb` landed; **fireball re-run required** | WS-07 + WS-08 |
| B5 | User params for colour, scale, intensity | **Assertions expanded** at `3b39dd3`; runtime pending | WS-07 + WS-11 |
| B6 | System compiles; compile genuinely awaited | **Assertions expanded** at `3b39dd3`; runtime pending | WS-07 + WS-11 |
| B7 | Structural validation: emitters non-empty, renderers bound, no missing DIs | **PASS scaffold** on `825e4f4` only — not overall POC-B | WS-07 |
| B8 | Assets saved and survive editor restart | **Filters landed, not proven** — `Restart.Create/Verify` at `163b272`; WS-11 must run two-process restart scenario | WS-11 |
| B9 | One structured response with complete change manifest | **Assertions expanded** at `3b39dd3`; runtime pending | WS-07 + WS-11 |
| B10 | Visibly renders as fireball when placed — screenshot supplementary only | **Not required as validation**; optional after B1–B9 | WS-07 / WS-11 |

Global POC rules still apply: real RE project; scratch under `/Game/__UeremcpPoc/`; metrics reported numerically; success with only `validate:false` does **not** count (`docs/POC_ACCEPTANCE.md`).

---

## Follow-ups (do not launch false claims)

| Priority | Follow-up | Owner | WS-01 action |
|---|---|---|---|
| P0 | Re-run CRT after BlueprintTools bootstrap `809f863` and selector correction `eccc282` | WS-11 | Prior A1/A2/A10 PASS; A5/A9 need fresh proof; refuse overall POC-A |
| P0 | Re-run fireball after ribbon fix `7a417bb` with expanded assertions `3b39dd3` | WS-11 | Fix and assertions landed; refuse overall POC-B |
| P0 | Run `run_poc_acceptance.ps1 -Scenario B8` (Create → restart → Verify) after `163b272` | WS-11 | Filters ready; refuse B8/POC-B until PASS |
| P0 | Full POC B fireball (MCP) covering B1–B9 after filter green | WS-07 (+ WS-08) + WS-11 | Track; refuse overall POC-B |
| P1 | Record measured metrics in `docs/reviews/poc-metrics.md` (E7) | WS-11 / WS-14 | WS-01 may scaffold the file when first real numbers exist — **empty metrics file is not a claim** |
| P2 | POC C / D / E remainder | WS-07+15 / WS-09 / WS-11 | Out of this residual’s critical path after A6 + POC B |

**Not claimed:** overall POC A, overall POC B, or POC-A/POC-B “essentially done.”

**Junction:** RE and VisualTest must remain on `UEREMCP-ws01\Plugins\UEREMCP`.
