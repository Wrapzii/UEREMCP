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

**Exit condition: R-01, R-03, R-04, R-06 are closed.** Nothing in Phase 2 starts until
they are, because all four can invalidate Phase 2 work.

| Deliverable | WS | Closes |
|---|---|---|
| Compiling plugin, one `AICallable` tool reachable from an MCP client | WS-03 | R-04 |
| Epic toolset inventory — the "do not rebuild" list | WS-02 | R-06 |
| `FileSandbox` semantics; `Rollback.MultiAssetDiscard` | WS-11 | R-03 |
| Blueprint graph **read** into `graph.schema.json` | WS-06 | R-01 (half) |
| Envelope parse/serialise/validate in C++ | WS-05 | — |
| Test harness, one passing integration test | WS-11 | R-14 |
| Transport capabilities and job model constraints | WS-04 | R-11 |

This phase is deliberately all *executable* deliverables. Phase 1 producing only
documents is R-15 materialising.

## Phase 2 — The core thesis

| Deliverable | WS | Notes |
|---|---|---|
| **POC A** — Blueprint round trip | WS-06 | The make-or-break deliverable |
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
