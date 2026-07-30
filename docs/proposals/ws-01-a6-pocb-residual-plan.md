# WS-01 residual plan — A6 and overall POC-B

- **Orch tip at writing:** `6b92576` (`5e3566d` Niagara change-manifest / B3–B9 gate scaffolding integrated)
- **Date:** 2026-07-30
- **Status:** Wave 2 listed editor filters are green; live VisualTest MCP T1a freshness is PASS. B7 scaffold remains green (`825e4f4` = prior `7394f49`). B9 compose-derived change-manifest scaffolding and honest `extra.poc_b_gates` for B3/B5/B6/B9 landed (`6b92576`). **A6 is not met. Overall POC-B is not met.** No junction retarget.

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
| End-to-end POC A scenario covering A1–A11 (or documented A8 escape via patch + A10) | WS-06 + WS-11 | Single editor/MCP run with programmatic asserts; metrics (`mcp_round_trips`, `internal_operations`, wall-clock, tokens); scratch under `/Game/__UeremcpPoc/` |
| Explicit A6 assert: after replace, re-read contains expected nodes + expected connections | WS-06 | Automation or MCP script that fails if nodes/links missing — not visual inspection |
| A7 fields on the success path | WS-06 | Response shows `modified_and_validated` and `validation.reread_after_write == true` |
| A8 / A11 identity and no-op | WS-06 | Identical `content_hash` on unchanged replace; second replace `no_change_required` without recompile |
| Metrics file for POC A | WS-11 / WS-14 | `docs/reviews/poc-metrics.md` (POC E7) |

**WS-01 next step:** none that can close A6 without editing WS-06 paths. Keep criteria frozen; refuse false claims; track evidence here / in filter-results.

---

## What overall POC-B still lacks beyond B7 / Wave 2 filters

POC B owner: **WS-07**, depends on **WS-08** for materials (`docs/POC_ACCEPTANCE.md`, `docs/WORK_ALLOCATION.md`).

Recorded Wave 2 evidence that is **in scope but insufficient for overall POC-B**:

| Evidence | Tip | Scope |
|---|---|---|
| `UEREMCP.Niagara.Create` PASS 10/10 | `2384112` | Create suite, not B1–B10 |
| `UEREMCP.Niagara.Inspect` PASS 4/4 | `2384112` | Inspect suite |
| `UEREMCP.Niagara.POCB.SixEmitterGateScaffold` (B7) PASS 1/1 | `825e4f4` (`7394f49` lineage) | **B7 structural scaffold only** (`proof=editor_create_reread_honesty`) |
| Niagara change-manifest + extended `poc_b_gates` (B3/B5/B6/B9) | `6b92576` (`5e3566d`) | **Scaffolding / offline tests only** — envelope `created_assets`/`reused_assets`/`changes[]`/`validation.reread_after_write` wired; not an end-to-end POC-B PASS |
| `UeremcpMaterial.Toolset` PASS 11/11 + live VisualTest MCP T1a | `7535e6c` | Material disk-save / validate:false honesty — supports B2/B8 inputs, does **not** complete POC B |

Niagara toolset comments still describe `create_niagara_effect` as a **POC B probe slice** and defer full `*_validated` until materials, renderer binding, and structural re-read are proven.

### POC B checklist (B1–B10)

| # | Criterion (short) | Status vs current evidence | Owner |
|---|---|---|---|
| B1 | One MCP request produces the complete effect — no follow-ups | **Still open** — no fireball envelope end-to-end acceptance run | WS-07 + WS-11 |
| B2 | Materials created or reused; reuse in `result.reused_assets` | **Still open** for POC B fireball — Material T1a / toolset green; create path can populate `reused_assets` but fireball proof missing | WS-08 + WS-07 |
| B3 | Niagara system exists with all six requested emitters | **Partial scaffolding** — honest B3 gates in `extra.poc_b_gates` (`6b92576`); needs fireball acceptance run | WS-07 |
| B4 | Renderers configured and bound to valid materials | **Still open** — B7 scaffold evaluates bind signals; full fireball bind verify not acceptance-proven | WS-07 + WS-08 |
| B5 | User params for colour, scale, intensity | **Partial scaffolding** — honest B5 gate fields (`6b92576`); acceptance run missing | WS-07 |
| B6 | System compiles; compile genuinely awaited | **Partial scaffolding** — honest B6 gate fields + prior AwaitCompile fixes; acceptance run missing | WS-07 |
| B7 | Structural validation: emitters non-empty, renderers bound, no missing DIs | **PASS scaffold** on `825e4f4` only — not overall POC-B | WS-07 |
| B8 | Assets saved and survive editor restart | **Still open** for the POC B fireball asset set | WS-07 + WS-11 (E1 overlap) |
| B9 | One structured response with complete change manifest | **Compose-derived manifest scaffolding landed** (`6b92576` / `5e3566d`): `created_assets` / `reused_assets` / `changes[]` + offline ChangeManifest tests. **Not** an end-to-end POC-B acceptance PASS | WS-07 |
| B10 | Visibly renders as fireball when placed — screenshot supplementary only | **Not required as validation**; optional after B1–B9 | WS-07 / WS-11 |

Global POC rules still apply: real RE project; scratch under `/Game/__UeremcpPoc/`; metrics reported numerically; success with only `validate:false` does **not** count (`docs/POC_ACCEPTANCE.md`).

---

## Follow-ups (do not launch false claims)

| Priority | Follow-up | Owner | WS-01 action |
|---|---|---|---|
| P0 | POC A scenario including explicit A6 programmatic re-read | WS-06 | Track; do not claim until WS-11 editor/MCP evidence lands |
| P0 | Full POC B fireball run covering B1–B9 (B7 scaffold green; B9 compose scaffolding landed — still not end-to-end) | WS-07 (+ WS-08 materials) | Track; refuse “POC B complete” from B7 or B9 scaffolding alone |
| P1 | Editor-restart survival for POC B assets (B8) and cross-POC E1 | WS-11 | Harness / live restart proof |
| P1 | Record measured metrics in `docs/reviews/poc-metrics.md` (E7) | WS-11 / WS-14 | WS-01 may scaffold the file when first real numbers exist — **empty metrics file is not a claim** |
| P2 | POC C / D / E remainder | WS-07+15 / WS-09 / WS-11 | Out of this residual’s critical path after A6 + POC B |

**Not claimed:** A6, overall POC A, overall POC B, A6/POC-B “essentially done.”

**Junction:** RE and VisualTest must remain on `UEREMCP-ws01\Plugins\UEREMCP`.
