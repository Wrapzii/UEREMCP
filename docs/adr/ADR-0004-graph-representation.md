# ADR-0004: Complete graph representation and exchange

- **Status:** Accepted
- **Date:** 2026-07-29
- **Owner:** WS-01
- **Unblocks:** WS-06 (Blueprint), WS-07 (Niagara), WS-08 (Material/VFX), WS-10 (Animation/Control Rig)
- **Depends on:** ADR-0003

## Context

The core inefficiency this project exists to remove is the
inspect-one-node / create-one-node / connect-one-pin / compile / repeat loop. Removing
it requires that a whole graph can be *retrieved* as structured data and *submitted*
as structured data.

Four different graph families are in scope — Blueprint (event/function/macro/
construction), Niagara (system/emitter/module stacks), Material, and Animation
Blueprint / Control Rig / State Machine / Behavior Tree / StateTree / PCG. They differ
enormously in their editor APIs, but agents should not need four mental models.

Whether full fidelity round-trip is achievable per family is **unresolved and is the
project's central technical risk** (`RB-05`, `RB-07`, `RB-08`, `RB-09`). This ADR
fixes the *representation*, not the achievability.

## Decision

One graph schema, `schemas/graph/graph.schema.json`, shared by all graph families,
with a `graph_type` discriminator and family-specific extension objects.

**Required for every graph, regardless of family:**

- identity: `asset_path`, `graph_id`, `graph_name`, `graph_type`
- integrity: `content_hash`, `revision`, `engine_version`, `schema_version`,
  `retrieved_at`
- structure: `nodes[]`, `links[]`
- per node: `node_id` (stable within the response), `node_class`,
  `semantic_type`, `position`, `properties`, `input_pins[]`, `output_pins[]`
- per pin: `pin_id`, `name`, `direction`, `pin_type`, `default_value`, `links[]`
- scope: `variables[]`, `functions[]`, `events[]`, `entry_points[]`, `exit_points[]`,
  `subgraphs[]`, `comments[]`
- `dependencies[]` and `unresolved_references[]`
- `diagnostics`: `dead_nodes`, `disconnected_subgraphs`, `invalid_links`,
  `missing_required_inputs`, `unused_outputs`, `type_mismatches`, `warnings`,
  `errors`
- `execution_paths[]` and `data_paths[]` where the family has them

**Two identifier layers, and the distinction matters:**

- `node_id` — stable *within one retrieval*, used for links inside that payload.
- `semantic_id` — optional, stable *across* retrievals and rebuilds, derived from
  role rather than engine object identity.

Engine-internal GUIDs are **not** the contract. Master prompt §2.5 is explicit that
correctness and repeatability outweigh preserving internal node identity, and
delete-and-recreate is an acceptable implementation of a modification. `semantic_id`
is what survives a rebuild; `node_id` is not expected to.

**Four modification modes**, matching ADR-0003's `mode`: `replace` (submit a complete
graph), `patch` (submit a semantic diff), `rebuild_from_specification` (submit intent,
not structure), `repair` (submit goals, let the service determine the change).

**Diagnostics are mandatory on retrieval, not opt-in.** An agent asking for a graph
is nearly always about to change it; withholding "this subgraph is disconnected"
until a second call reintroduces the round-trip problem this project exists to solve.

**Retrieval is not all-or-nothing.** `response_detail` gates payload size:
`summary` returns identity, entry points, semantic summary, and diagnostics only;
`complete` returns full nodes and links. A 4,000-node Blueprint must not be the
default response.

## Alternatives considered

| Alternative | Why rejected |
|---|---|
| A separate schema per graph family | Four schemas, four validators, four agent mental models, and cross-domain batches become untypeable. Family differences are handled by extension objects instead. |
| Round-trip Unreal's native graph serialization (`T3D`/copy-paste text) | Opaque to agents, not schema-validatable, no diagnostics, no semantic layer. **Worth researching as an internal implementation detail** for fidelity — `RB-05` — but not as the agent contract. |
| Preserve engine GUIDs as the stable identity | Fights the delete-and-recreate strategy the master prompt explicitly endorses, and forces granular editing exactly where it is least reliable. |
| Diagnostics on request only | Reintroduces a round trip in the common case. |
| Screenshot or node-name list | Explicitly prohibited by master prompt §2.4 and §26. |

## Consequences

**Enables:** one agent mental model across Blueprint, Niagara, Material, and
animation graphs. Cross-family batches are expressible. One validator, one differ,
one patch engine, shared by all domain workstreams.

**Costs:** the common denominator will fit some families awkwardly — Niagara module
stacks and Material expression graphs are not naturally the same shape as a Blueprint
event graph, and forcing them into `nodes`/`links` risks a lossy or unnatural mapping.
This is the main thing domain workstreams must push back on early, via
`docs/proposals/`, **before** they build against it. If two families genuinely cannot
share, that is an ADR-0004 challenge with evidence — and a legitimate one.

**Locks in:** the identifier model. Changing `semantic_id` semantics later invalidates
every stored template's `construction_plan`.

## Open questions

Assigned, not forgotten:

- Can a Blueprint event graph be fully reconstructed from this representation, and
  where does fidelity break — delegates, latent nodes, custom K2 nodes, macro
  instances? (`RB-05`)
- Niagara module stacks: are they expressible as `nodes`/`links`, or do they need a
  first-class stack extension? (`RB-07`)
- Control Rig and AnimBP state machines: what is even readable via public API? (`RB-09`)
- What is `content_hash` computed over such that it is stable across irrelevant
  reordering but sensitive to real change? (`WS-05`)

## Verification

Round-trip test, per family, in each domain workstream's test suite: retrieve →
submit unchanged with `mode: replace` → retrieve again → assert `content_hash`
unchanged and diagnostics unchanged. A family that cannot pass this does not claim
round-trip support.
