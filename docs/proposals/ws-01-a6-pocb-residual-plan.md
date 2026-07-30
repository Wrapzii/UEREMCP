# WS-01 residual plan — A6 and overall POC-B

- **Orch tip at writing:** `674c439` (WS-11 fireball result parsing fix after runtime re-proof)
- **Date:** 2026-07-30
- **Status:** Runtime acceptance follow-up on `d691316` **failed**. `PocA6Reread` returned `failed_validation`: BeginPlay→Branch link missing; A8/A11 no-op failed. Fireball POC created the system under the POC root, but MIs remained under the test root; B4 was false and B2 hit a harness manifest-path issue. Material Toolset **PASS 14/14**. WS-11 parsing fix `656ffe0` is integrated as `674c439`. **No A6 / POC A / B1 / B2 / B4 / overall POC-B claim.**

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

Filter-results already declines an A6 claim. That remains correct.

### Missing A6 / POC A evidence (owners)

| Gap | Owner | Evidence needed |
|---|---|---|
| **A6 code on orch** | WS-06 | Landed `13bf529` (`2a0b2cd`): programmatic reread-after-write validation + harness handoff `docs/proposals/ws-06-a6-ws11-harness.md`. **Not an A6 claim.** |
| Editor/runtime A6 filter PASS | WS-11 (+ WS-06) | **FAIL on `d691316`** — BeginPlay→Branch link missing; A8/A11 no-op failed. Log: `editor_UeremcpBlueprint_Toolset_PocA6Reread_20260730_050942.log` |
| End-to-end POC A (A1–A11) or A8 patch escape + A10 | WS-06 + WS-11 | Full scenario metrics under `/Game/__UeremcpPoc/` |
| A7 / A8 / A11 fields on success path | WS-06 | Proven only after editor run |
| Metrics file for POC A | WS-11 / WS-14 | `docs/reviews/poc-metrics.md` (POC E7) |

**WS-01 next step:** WS-06 fixes the missing execution link and A8/A11 no-op behavior; WS-11 re-runs. Refuse A6/POC-A claims until PASS.

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
| `UeremcpMaterial.Toolset` PASS 11/11 + live VisualTest MCP T1a | `7535e6c` | Disk-save / validate:false honesty |

### POC-root allowlist status

Prior blocker (`docs/proposals/ws-11-pocb-poc-root-blocker.md`): Niagara/Material rejected `/Game/__UeremcpPoc/`.

**Runtime result on `d691316`:** Niagara system path was under `/Game/__UeremcpPoc/`, but generated MIs still resolved under `/Game/__UeremcpTests/`; B4 was false. B2 also encountered a harness manifest-path issue (parsing fix now landed at `674c439`). Log: `editor_UEREMCP_Niagara_POCB_FireballInlineMaterials_20260730_050816.log`.

### POC B checklist (B1–B10)

| # | Criterion (short) | Status vs current evidence | Owner |
|---|---|---|---|
| B1 | One MCP request produces the complete effect — no follow-ups | **Still open** — filter is one direct editor tool call, not MCP; live fireball MCP missing | WS-07 + WS-11 |
| B2 | Materials created or reused; reuse in `result.reused_assets` | **FAIL / not proven** — MIs under test root; harness manifest-path issue (parser fixed, domain co-location still open) | WS-08 + WS-07 |
| B3 | Niagara system exists with all six requested emitters | **Partial scaffolding** — needs fireball acceptance run | WS-07 |
| B4 | Renderers configured and bound to valid materials | **FAIL** on `d691316` — `B4=false`; MI co-location under POC root still open | WS-07 + WS-08 |
| B5 | User params for colour, scale, intensity | **Partial scaffolding** — acceptance run missing | WS-07 |
| B6 | System compiles; compile genuinely awaited | **Partial scaffolding** — acceptance run missing | WS-07 |
| B7 | Structural validation: emitters non-empty, renderers bound, no missing DIs | **PASS scaffold** on `825e4f4` only — not overall POC-B | WS-07 |
| B8 | Assets saved and survive editor restart | **Still open** | WS-07 + WS-11 |
| B9 | One structured response with complete change manifest | **Compose scaffolding landed**; not end-to-end POC-B PASS | WS-07 |
| B10 | Visibly renders as fireball when placed — screenshot supplementary only | **Not required as validation**; optional after B1–B9 | WS-07 / WS-11 |

Global POC rules still apply: real RE project; scratch under `/Game/__UeremcpPoc/`; metrics reported numerically; success with only `validate:false` does **not** count (`docs/POC_ACCEPTANCE.md`).

---

## Follow-ups (do not launch false claims)

| Priority | Follow-up | Owner | WS-01 action |
|---|---|---|---|
| P0 | POC A scenario including explicit A6 programmatic re-read | WS-06 | Track; do not claim until WS-11 editor/MCP evidence lands |
| P0 | Fix BeginPlay→Branch link and A8/A11 no-op, then re-run A6 | WS-06 + WS-11 | Current runtime result FAIL; refuse A6/POC-A |
| P0 | Co-locate generated MIs with Niagara system under POC root; then re-run fireball | WS-07 + WS-08 + WS-11 | Current fireball result FAIL; refuse B1/B2/B4 |
| P0 | Full POC B fireball (MCP) covering B1–B9 after filter green | WS-07 (+ WS-08) + WS-11 | Track; refuse overall POC-B |
| P1 | Editor-restart survival for POC B assets (B8) and cross-POC E1 | WS-11 | Harness / live restart proof |
| P1 | Record measured metrics in `docs/reviews/poc-metrics.md` (E7) | WS-11 / WS-14 | WS-01 may scaffold the file when first real numbers exist — **empty metrics file is not a claim** |
| P2 | POC C / D / E remainder | WS-07+15 / WS-09 / WS-11 | Out of this residual’s critical path after A6 + POC B |

**Not claimed:** A6, overall POC A, overall POC B, A6/POC-B “essentially done.”

**Junction:** RE and VisualTest must remain on `UEREMCP-ws01\Plugins\UEREMCP`.
