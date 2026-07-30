# Implementation Roadmap

**Owner:** WS-01. Phases are dependency order, not dates. Nothing here permits
excluding later domains from the *design* — master prompt §24 is explicit on that, and
§9 forbids deferring animation to an undefined future system.

## Departure from the master prompt's sequence

The master prompt lists Niagara as one of the highest-priority domains and sequences
Niagara inspection/creation before Blueprint round-trip in some readings.

**This roadmap puts Blueprint round-trip first.** Reasoning, recorded so it is
challengeable (`docs/WHY.md`):

- It is the cheapest way to find out whether the central thesis holds (R-01).
- It addresses the owner's stated "micromanaging nodes on characters" pain directly.
- Niagara module stacks are the **least** node-graph-shaped domain in scope. Validating
  the shared graph representation on the hardest case first is the wrong order.

Niagara follows immediately and loses nothing — it gains WS-06's serialisation
machinery.

---

## Phase 0 — Foundation (complete)

Grounded facts, ADRs 0001–0006, core schemas, work allocation, research briefs, POC
criteria, risk register, plugin scaffold. Frozen.

## Phase 1 — Prove the architecture

**Status: exited 2026-07-30** (WS-01 decision — mitigated gates; see below).

**Original exit condition:** R-01, R-03, R-04, R-06 **closed**.

**Actual exit (2026-07-30):** architecture de-risk complete with two gates **mitigated**, two **closed** (R-04, R-06 schema-matrix). Wave 2 implementation is authorized; first kick is WS-06 P0
(`docs/proposals/ws-06-p0-authorized.md`).

| Gate | Phase 1 verdict | Evidence |
|---|---|---|
| R-04 | **Closed** | MCP Ping/Echo on RE `[VERIFIED-RUNTIME: 127.0.0.1:8000, 2026-07-30]` |
| R-03 | **Mitigated** | Shipping Validation Rollback Content/ full-Discard green on RE |
| R-01 | **Mitigated** | RB-05: public-API reconstruction tractable (Epic DSL + primitives); scratch BP DSL round-trip `[VERIFIED-RUNTIME: RB-05 visualtest]`—**not** `graph.schema.json`/`content_hash`. Residual: envelope bridge = POC A (Wave 2) |
| R-06 | **Closed** | 73/73 `describe_toolset` dumps, 938 tools on RE 2026-07-30 (`8cea492`); `epic-toolsets.md` / `reagenttools.md` `runtime_complete`. Follow-ons: payload calibration, async classification, cutover bar, RECapture GIF helpers |

**Why mitigated suffices for R-01:** Phase 1 was to answer whether the central thesis
can hold — not to ship POC A. RB-05 answers the thesis risk ("cannot reconstruct via
public API") **no**: Epic `BlueprintTools` + `write_graph_dsl` reconstruct a large,
well-defined subset today. The UEREMCP gap is envelope + stable IDs/hashes/verification
over that substrate, which is exactly POC A scope.

**R-06 closed (schema-matrix):** Rule 2 ("audit before you build") is operational with full runtime backing: every toolset in `list_toolsets` has a `describe_toolset` dump under `docs/audit/raw/schemas/` (73/73, 938 tools, `8cea492`). Residual payload/async/cutover items are follow-ons, not duplicate-check gates.

| Deliverable | WS | Status |
|---|---|---|
| Compiling plugin, one `AICallable` tool reachable from an MCP client | WS-03 | **Done** — R-04 closed |
| Epic toolset inventory — the "do not rebuild" list | WS-02 | **Done** — R-06 closed; runtime matrix in `docs/audit/raw/schemas/` |
| `FileSandbox` semantics; `Rollback.MultiAssetDiscard` | WS-11 | **Done (mitigated)** — R-03 mitigated |
| Blueprint graph **read** into `graph.schema.json` | WS-06 | **Research done** — RB-05; impl deferred to POC A (Wave 2) |
| Envelope parse/serialise/validate in C++ | WS-05 | **Done** — `ws-05-protocol` |
| Test harness, one passing integration test | WS-11 | Landed; R-14 still open |
| Transport capabilities and job model constraints | WS-04 | Partial — ADR-0009 accepted |

This phase is deliberately all *executable* deliverables. Phase 1 producing only
documents is R-15 materialising.

## POC claim status (2026-07-30)

**POC A–E claimed** against `docs/POC_ACCEPTANCE.md` on the post-hardening local tip
(parent of documentation certification). Live closeouts include D5 multi-client,
B10 rendered warm-pixel, Niagara/Material Domain E3/E4, durable `execute_plan`
idempotency, and cooperative `cancel_job`.

**Production-ready: No.** Remaining: Epic MCP `notifications/cancelled` immutable
adapter limitation; durable-idempotency crash/migration caveats; metrics gaps (R-17);
experimental engine churn (R-02); domains beyond wired mutators. See
`docs/proposals/ws-01-hardening-consolidation-2026-07-30.md` and
`docs/proposals/ws-01-documentation-certification-2026-07-30.md`.

## Phase 2 — The core thesis

**Authorized 2026-07-30.** Start with WS-06 P0 (`docs/proposals/ws-06-p0-authorized.md`).
**POC A path claimed** on tip (scoped CRT — not arbitrary complex graphs).

| Deliverable | WS | Notes |
|---|---|---|
| **POC A** — Blueprint round trip | WS-06 | **Claimed** (scoped); patch mode / exotic islands residual |
| Blueprint semantic analysis and `repair_blueprint` | WS-06 | Master prompt §7.2 — still planned |
| Batch executor: dependency graph, `$ref`, atomic, rollback | WS-05 | `execute_plan` partial; durable Claim/Complete landed |
| Validation framework: check registry, re-read-after-write | WS-11 | Landed for POC gates; R-14 still open |
| Niagara **read** into `graph.schema.json` + `extensions.niagara` | WS-07 | Landed (intentionally lossy topology) |
| VFX material authoring; master material set | WS-08 | Landed for POC B dependency |
| Permission tiers, allowed roots, audit log | WS-12 | **Mitigated** — R-07/R-12; see RISK_REGISTER |

## Phase 3 — Semantic creation

**POC B and POC C claimed** (structural + B10 warm-pixel; variation + C7). Metrics
overall close (E7 / R-17) **not** claimed.

| Deliverable | WS | Status |
|---|---|---|
| **POC B** — goal-level Niagara creation | WS-07 + WS-08 | **Claimed** — B10 `VisibleRender` PASS; production visual perfection not claimed |
| Procedural texture / helper mesh generation | WS-08 | Partial — test-root AICallable |
| Template library: store, search, instantiate, promote | WS-15 | Partial — AICallable; used by POC C |
| **POC C** — variation from existing effect, incl. third-generation instantiation | WS-07 + WS-15 | **Claimed** |
| Capability discovery layered over `list_toolsets` | WS-05 | Still planned (`list_domains`, …) |

## Phase 4 — Breadth

**POC D and POC E claimed** (D5 live multi-client + Domain E3/E4). Animation /
full GAS textbook surface remain partial/research.

| Deliverable | WS | Status |
|---|---|---|
| GAS: abilities, effects, tags, cues, replication validation | WS-09 | Partial — RE Pattern B / create_spell path |
| **POC D** — batched gameplay ability | WS-09 | **Claimed** — D5 live multi-client proof |
| Player-system goal operations (movement, interaction, inventory, spellcasting) | WS-09 | Beyond POC D scope |
| Animation: sequences, montages, notifies, AnimBP graphs | WS-10 | Partial inspect / read; authoring unsupported |
| Control Rig / IK to the ceiling RB-09 establishes | WS-10 | Research — may prove read-only |
| **POC E** — durability and honesty | WS-11 | **Claimed** — Domain E3/E4 + restart durable pair |

## Phase 5 — Consolidation

Hardening slice landed 2026-07-30 (cancel, durable idempotency, D5/B10). Remaining
Phase 5 work is production breadth, not POC reopen.

| Deliverable | WS | Status |
|---|---|---|
| Long-running job model per ADR-0009 | WS-04 | Partial — `get_job_result` / cooperative `cancel_job` available; Epic protocol cancel immutable limit |
| Performance: payload trimming, caching, compile batching | WS-05 | Open |
| Security hardening per ADR-0010 | WS-12 | **Mitigated** for wired mutators (R-07/R-12); residual unwired paths |
| Benchmark report vs the ~5:1 baseline | WS-11 | Open (R-17); many metrics cells `unavailable` |
| Agent usage docs, capability catalog, template authoring guide | WS-13 | Landed under `docs/guide/**` — keep current with catalog |
| Full compatibility review vs Epic + REAgentTools coverage | WS-02 + WS-14 | Partial — schema-matrix closed; cutover bar open |
| REAgentTools cutover | WS-02 | Open |

## Later domains — designed now, built later

World building, level design, PCG, AI/behaviour trees, StateTree, UMG, audio, sequencer,
source control, data assets.

These are **in the architecture already**: `graph.schema.json` covers `BehaviorTree`,
`StateTree`, and `PCGGraph`; `template.schema.json`'s `domain` enum includes all of
them; the envelope is domain-agnostic. Adding one should require a new domain module and
a `specification` schema — **no protocol change**. If adding a domain forces a protocol
change, the architecture failed and that is an ADR challenge.

## Measurement is not a phase

`metrics` are mandatory from the first tool (ADR-0003). Benchmark against the REAgentTools
baseline from Phase 2 onward, not at the end. R-17 — discovering in Phase 5 that the gain
does not beat 5:1 — is only avoidable by measuring early.
