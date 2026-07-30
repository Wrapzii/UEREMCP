# Proof-of-Concept Acceptance Criteria

**Owner:** WS-01. Criteria are binary — met or not met. No partial credit, no
"essentially working."

## Why these are strict

Master prompt §26 lists "an architecture document with no executable proof of concept"
as a prohibited outcome. The POCs are the only thing that distinguishes this project
from a pile of design documents.

Every criterion below is written so that **an agent cannot claim it without having
actually run it.** If you cannot demonstrate a criterion, mark it not met and say why.
A truthful "POC A criterion 6 fails on latent nodes" is a valuable result. A false
"POC A complete" poisons every downstream decision.

## Global rules for all POCs

- Every POC runs against the real RE project, not a toy project.
- Scratch assets go under `/Game/__UeremcpPoc/`, cleaned up on completion.
- Every POC records `metrics.mcp_round_trips`, `metrics.internal_operations`, total
  tokens, and wall-clock time. **Report the numbers, not adjectives.**
- Every POC states the equivalent primitive-call count it replaces, so the reduction is
  measured against the ~5:1 REAgentTools baseline (`docs/WHY.md`).
- A POC that succeeds only with `options.validate: false` has not succeeded.

---

## POC A — Complete Blueprint round trip

**Owner:** WS-06. **Priority: first.** See `docs/WHY.md` for why this precedes Niagara.

This is the make-or-break deliverable. If it passes, the thesis holds. If it fails, the
architecture degrades to inspection-plus-patching and the roadmap changes — which we
need to know in week one, not week six.

**Architecture (RB-05):** compose Epic `BlueprintTools` DSL/primitives under
`blueprints.read_graph` / `blueprints.submit_graph` — do **not** rebuild pin
authoring. `content_hash` is over structured canonical JSON (`semantic_id`), not
DSL text. Documented lossy: MultiGate decompile-on-exec, Timeline spawn syntax,
bind elision. Patch mode is the A8 escape hatch (`ws-06-patch-mode-and-impl-plan.md`).
Implementation is authorized after Phase 1 mitigated exit (2026-07-30 per `docs/ROADMAP.md`): R-04 closed; R-01, R-03, and R-06 mitigated. WS-06 P0 may proceed; criteria A1–A11 below remain the POC A proof bar (including the `graph.schema.json` envelope bridge).

| # | Criterion |
|---|---|
| A1 | Retrieve a complete Blueprint graph as JSON conforming to `schemas/graph/graph.schema.json` in **one** MCP call |
| A2 | The payload contains nodes, pins, pin types, pin defaults, links, variables, entry points, and dependencies — sufficient to reconstruct the graph |
| A3 | `diagnostics` correctly identifies at least one deliberately introduced dead node and one disconnected subgraph |
| A4 | Modify the JSON externally (add a branch and a function call, wire execution through it) |
| A5 | Submit with `mode: replace` in **one** MCP call; the operation compiles and saves |
| A6 | Re-read the graph and confirm the expected nodes exist and the expected connections are present — asserted programmatically, not by eye |
| A7 | `status` is `modified_and_validated`, and `validation.reread_after_write` is `true` |
| A8 | Round-trip identity: retrieve → replace unchanged → retrieve yields an **identical `content_hash`** |
| A9 | The whole scenario completes in ≤ 3 MCP calls |
| A10 | `fidelity.lossy_areas` honestly lists anything not captured |
| A11 | A second identical `replace` returns `no_change_required` and does not recompile |

**If A8 cannot pass** for complex graphs, POC A is still met at the simple-graph level
provided A10 documents the boundary precisely and a `patch` mode is demonstrated that
achieves A4–A7 without whole-graph replace.

---

## POC B — Goal-level Niagara creation

**Owner:** WS-07, depends on WS-08 for materials.

Request: the fireball specification in `schemas/envelope/request.schema.json`'s example
— `effect_type: projectile`, components `core`, `flame_shell`, `sparks`, `smoke`,
`ribbon_trail`, `impact_burst`, with colours, scale, intensity.

| # | Criterion |
|---|---|
| B1 | One MCP request produces the complete effect — no follow-up calls |
| B2 | Required materials are created or reused; reuse is reported in `result.reused_assets` |
| B3 | The Niagara system exists with all six requested components as emitters |
| B4 | Renderers are configured and bound to valid materials |
| B5 | User parameters are exposed for colour, scale, and intensity |
| B6 | The system compiles, with compilation genuinely awaited — not assumed |
| B7 | Structural validation passes: emitters non-empty, renderers bound, no missing data interfaces |
| B8 | Assets are saved and survive an editor restart |
| B9 | One structured response with a complete change manifest |
| B10 | It visibly renders as a fireball when placed — screenshot as **supplementary** evidence only, never as the validation itself |

---

## POC C — Variation from an existing effect

**Owner:** WS-07 + WS-15.

Request, as **one** operation: create an ice variation of the fireball — reuse
structural logic, change materials and parameters, add crystalline particles, change
the impact, preserve shared behaviour, save as a reusable template or variation.

| # | Criterion |
|---|---|
| C1 | One MCP request, no follow-ups |
| C2 | Structural logic is demonstrably reused, not copy-pasted wholesale — the response shows what was inherited vs overridden |
| C3 | Materials and parameters are changed as requested |
| C4 | The crystalline component is added |
| C5 | Networking and damage behaviour from the source are preserved and verified unchanged |
| C6 | The result is stored as a reusable template conforming to `schemas/template-library/template.schema.json` |
| C7 | The template can then instantiate a **third** variation, proving it is a pattern and not a copy |

C7 is the criterion that distinguishes a template library from a duplicate-assets
folder. Do not skip it.

---

## POC D — Batched RE spell (magecraft, not textbook GAS)

**Owner:** WS-09. Proves the architecture is not Niagara-only.

RE does **not** use Epic GAS for player abilities. Abilities are `FREAbilityDef`
rows in `DT_Abilities`, executed by `CastAbility` / `AuthorityCastAbility`
`[VERIFIED: RB-12]`. POC D targets that system. Textbook
`create_gameplay_ability` / `create_gameplay_effect` assets that the project
cannot cast are a **false POC** and are out of scope for RE.

Elemental variants (fire/frost/storm/nature/…) share **one** parameterized
`create_spell` (inputs/modifiers), not per-element primitive tools (ADR-0008).

| # | Criterion |
|---|---|
| D1 | One `execute_plan` upserts an ability **row** (+ VFX / optional spell-VFX definition) under `/Game/__UeremcpTests/` via `create_spell` / `upsert_ability_row` |
| D2 | Operations execute in dependency order derived from `depends_on` |
| D3 | `$ref` substitution correctly passes earlier results into later operations |
| D4 | RE identity fields are set (`Element` / `EffectTag` / `ImpactStatus` as applicable). Gameplay-tag INI mutation is **not** required for RE POC D |
| D5 | Replication follows RE Pattern B: static checklist + optional `pie_cast_and_capture`; multi-client net proof is RB-14 / WS-11, not a silent skip |
| D6 | Everything compiles and saves; DataTable row is re-readable and matches the request |
| D7 | A deliberately failed operation triggers rollback; no partial test assets remain |
| D8 | One consolidated response with per-operation results |

Production `DT_Abilities` rebuild scripts that delete-and-recreate the whole table
are **unsafe** for agent upsert (ADR-0006) — tools must row-upsert under test paths.

---

## POC E — Durability and honesty

**Owner:** WS-11. Cross-cutting. This is the POC that catches lying.

| # | Criterion |
|---|---|
| E1 | All POC A–D results survive an editor restart |
| E2 | `Rollback.MultiAssetDiscard` passes — ADR-0005's gate on every rollback claim |
| E3 | `Idempotency.RepeatedCreate` passes |
| E4 | `Revision.StaleRejected` passes |
| E5 | A request with `options.validate: false` returns `partially_completed`, **never** a `*_validated` status |
| E6 | A deliberately broken request produces `failed_validation` with actionable diagnostics — not a false success |
| E7 | Every POC's measured metrics are recorded in `docs/reviews/poc-metrics.md` |

---

## The headline scenario

Not a POC gate — the target that tells you the project succeeded:

> Inspect this player spell system, return everything relevant in one structured
> response, identify what is broken, add a new advanced fire spell based on the existing
> fireball, create any missing Niagara and material patterns, connect it to the gameplay
> ability system, configure replication, compile everything, validate the result, and
> return the complete change manifest.

One logical job. Track progress toward it in `docs/reviews/`.
