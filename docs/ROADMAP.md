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

**Actual exit (2026-07-30):** architecture de-risk complete with three gates **mitigated**
and one **closed**. Wave 2 implementation is authorized; first kick is WS-06 P0
(`docs/proposals/ws-06-p0-authorized.md`).

| Gate | Phase 1 verdict | Evidence |
|---|---|---|
| R-04 | **Closed** | MCP Ping/Echo on RE `[VERIFIED-RUNTIME: 127.0.0.1:8000, 2026-07-30]` |
| R-03 | **Mitigated** | Shipping Validation Rollback Content/ full-Discard green on RE |
| R-01 | **Mitigated** | RB-05: public-API reconstruction tractable (Epic DSL + primitives); scratch round-trip identical; bridge plan accepted. Residual: `graph.schema.json` envelope bridge = POC A (Wave 2) |
| R-06 | **Mitigated** | Source audit complete; 73 `list_toolsets` + 12 priority `describe_toolset` dumps on RE; disposition matrix schema-complete for cited toolsets. Residual: ~61 non-priority dumps, REAgentTools 15 runtime schemas, payload calibration |

**Why mitigated suffices for R-01:** Phase 1 was to answer whether the central thesis
can hold — not to ship POC A. RB-05 answers the thesis risk ("cannot reconstruct via
public API") **no**: Epic `BlueprintTools` + `write_graph_dsl` reconstruct a large,
well-defined subset today. The UEREMCP gap is envelope + stable IDs/hashes/verification
over that substrate, which is exactly POC A scope.

**Why mitigated suffices for R-06:** Rule 2 ("audit before you build") is operational:
`docs/audit/epic-toolsets.md` dispositions every priority domain toolset with source +
runtime schema evidence for the 12 toolsets the matrix cites. Dumping all 73
`describe_toolset` payloads is enrichment for payload calibration, not a duplicate-check
gate.

| Deliverable | WS | Status |
|---|---|---|
| Compiling plugin, one `AICallable` tool reachable from an MCP client | WS-03 | **Done** — R-04 closed |
| Epic toolset inventory — the "do not rebuild" list | WS-02 | **Done (mitigated)** — R-06 mitigated; enrichment tracked in `docs/audit/epic-toolsets.md` |
| `FileSandbox` semantics; `Rollback.MultiAssetDiscard` | WS-11 | **Done (mitigated)** — R-03 mitigated |
| Blueprint graph **read** into `graph.schema.json` | WS-06 | **Research done** — RB-05; impl deferred to POC A (Wave 2) |
| Envelope parse/serialise/validate in C++ | WS-05 | **Done** — `ws-05-protocol` |
| Test harness, one passing integration test | WS-11 | Landed; R-14 still open |
| Transport capabilities and job model constraints | WS-04 | Partial — ADR-0009 accepted |

This phase is deliberately all *executable* deliverables. Phase 1 producing only
documents is R-15 materialising.

## Phase 2 — The core thesis

**Authorized 2026-07-30.** Start with WS-06 P0 (`docs/proposals/ws-06-p0-authorized.md`).

| Deliverable | WS | Notes |
|---|---|---|
| **POC A** — Blueprint round trip | WS-06 | The make-or-break deliverable; P0 scaffolding first |
| Blueprint semantic analysis and `repair_blueprint` | WS-06 | Master prompt §7.2 |
| Batch executor: dependency graph, `$ref`, atomic, rollback | WS-05 | Audit `execute_editor_batch` first (RB-15 q6) |
| Validation framework: check registry, re-read-after-write | WS-11 | The engine of rule 6 |
| Niagara **read** into `graph.schema.json` + `extensions.niagara` | WS-07 | Verdict on R-05 |
| VFX material authoring; master material set | WS-08 | POC B dependency |
| Permission tiers, allowed roots, audit log | WS-12 | R-07 |

## Phase 3 — Semantic creation

| Deliverable | WS |
|---|---|
| **POC B** — goal-level Niagara creation | WS-07 + WS-08 |
| Procedural texture / helper mesh generation | WS-08 |
| Template library: store, search, instantiate, promote | WS-15 |
| **POC C** — variation from existing effect, incl. third-generation instantiation | WS-07 + WS-15 |
| Capability discovery layered over `list_toolsets` | WS-05 |

## Phase 4 — Breadth

| Deliverable | WS |
|---|---|
| GAS: abilities, effects, tags, cues, replication validation | WS-09 |
| **POC D** — batched gameplay ability | WS-09 |
| Player-system goal operations (movement, interaction, inventory, spellcasting) | WS-09 |
| Animation: sequences, montages, notifies, AnimBP graphs | WS-10 |
| Control Rig / IK to the ceiling RB-09 establishes | WS-10 |
| **POC E** — durability and honesty | WS-11 |

## Phase 5 — Consolidation

| Deliverable | WS |
|---|---|
| Long-running job model per ADR-0009 | WS-04 |
| Performance: payload trimming, caching, compile batching | WS-05 |
| Security hardening per ADR-0010 | WS-12 |
| Benchmark report vs the ~5:1 baseline | WS-11 |
| Agent usage docs, capability catalog, template authoring guide | WS-13 |
| Full compatibility review vs Epic + REAgentTools coverage | WS-02 + WS-14 |
| REAgentTools cutover | WS-02 |

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
