# WS-10 → WS-06: share UEdGraph read for AnimBP node/link dump

- **From:** WS-10 (Animation & Control Rig)
- **To:** WS-06 (Blueprint)
- **Also notify:** WS-01 (if a shared module / ownership move is preferred over
  Animation → Blueprint module dependency)
- **Date:** 2026-07-30
- **Status:** open — blocks full `read_anim_bp` ADR-0004 emission; inventory
  workaround is live
- **Refs:** ADR-0004, RB-05, RB-09, `schemas/graph/graph.schema.json`,
  `docs/proposals/ws-06-content-hash-semantic-id.md`,
  `docs/proposals/ws-06-p0-authorized.md`

## What I need

A **read-only** shared UEdGraph → `graph.schema.json` walker that WS-10 can call
from `UeremcpAnimation` without forking Blueprint DSL or re-implementing pin/link
enumeration.

Concrete ask (prefer A, accept B):

### Option A — preferred for Wave 3

Expose a public C++ reader from `UeremcpBlueprint` (or promote later to a shared
helper if WS-01 prefers) with roughly this surface:

```cpp
// Pseudocode — WS-06 owns names/placement
struct FUeremcpEdGraphReadOptions
{
	FString GraphType;          // e.g. AnimBlueprintGraph / AnimStateMachine / BlueprintEventGraph
	bool bEmitNodesAndLinks;    // false => identity + counts only
	bool bIncludePinDefaults;
};

bool ReadEdGraph(
	const UEdGraph* Graph,
	const FString& AssetPath,
	const FUeremcpEdGraphReadOptions& Options,
	TSharedPtr<FJsonObject>& OutGraphJson,   // validates against graph.schema.json shape
	FString& OutError);
```

Requirements for the shared layer:

1. Walk `UEdGraph::Nodes`, pins, and links into ADR-0004 `nodes[]` / `links[]`.
2. Assign retrieval-local `node_id` / `pin_id`; **do not** treat engine `NodeGuid` /
   `GraphGuid` / `PinId` as contract identity
   `[VERIFIED: docs/adr/ADR-0004-graph-representation.md:47-56]`.
3. Leave `semantic_id` / `semantic_type` / family `extensions` hooks for the caller
   (callback or post-pass) so WS-10 can map `UAnimGraphNode_*` without WS-06 knowing
   AnimGraph types.
4. Omit positions / engine GUIDs from the hashed form per
   `ws-06-content-hash-semantic-id.md` and existing `CONTENT_HASH` ignore rules
   (`*_guid`, `node_id`, `pin_id`, `position`, …).
5. Set `fidelity.round_trip_supported=false` until that family's retrieve→replace→
   retrieve test exists (AnimBP authoring remains unsupported on WS-10 surface).

### Option B — acceptable interim

Document a frozen internal contract WS-10 may copy *temporarily* only if Option A
cannot land before POC A P1, and schedule deletion once the shared reader exists.
Prefer not to do this — forked walkers diverge.

## Why

AnimBP graphs are `UEdGraph` subclasses, not Blueprint DSL:

- `UAnimationGraph : UEdGraph`
  `[VERIFIED: Engine/Source/Editor/AnimGraph/Public/AnimationGraph.h:20]`
- `UAnimationStateMachineGraph : UEdGraph`
  `[VERIFIED: Engine/Source/Editor/AnimGraph/Public/AnimationStateMachineGraph.h:16]`
- Enumeration: `UAnimationBlueprintLibrary::GetAnimationGraphs` +
  `UBlueprint::GetAllGraphs`
  `[VERIFIED: AnimationBlueprintLibrary.h:681]`
  `[VERIFIED: Blueprint.h:1107]`

Runtime evidence that structure is listable but DSL is empty for Anim graphs:

- `BlueprintTools.list_graphs` on `ABP_Unarmed` returns AnimGraph, nested state
  machines, per-state graphs, transitions, EventGraph
  `[VERIFIED-RUNTIME: BlueprintTools.list_graphs on ABP_Unarmed]`
- `BlueprintTools.read_graph_dsl` on AnimGraph and Locomotion returns `""`
  `[VERIFIED-RUNTIME: read_graph_dsl AnimGraph + Locomotion → ""]`

Therefore ADR-0004 mapping must be a **UEdGraph walk shared with WS-06**, with an
AnimGraph-specific semantic layer owned by WS-10 — not DSL reuse
`[VERIFIED: docs/research/RB-09-animation-controlrig.md:67-84]`.

`schemas/graph/graph.schema.json` already discriminates `AnimBlueprintGraph` and
`AnimStateMachine` `[VERIFIED: schemas/graph/graph.schema.json:22-23]`. WS-10 must
not invent a parallel graph schema.

WS-06 P0 is authorized for scaffolding only; C++ graph walk is explicitly P1+ /
POC A `[VERIFIED: docs/proposals/ws-06-p0-authorized.md:34-38]`. This proposal
asks that when that walk lands, it is designed as **shareable**, not Blueprint-only.

## Ownership split (proposed)

| Layer | Owner | Notes |
|---|---|---|
| Generic UEdGraph node/pin/link serialization | WS-06 | Shared reader |
| Blueprint Event/Function/Macro semantic_id | WS-06 | Existing plan |
| AnimBP `EventGraph` | WS-06 path via shared reader | Same K2 graph family |
| AnimBP variables | Coordinate with WS-06 `list_variables` path | Do not fork |
| `UAnimGraphNode_*` → `semantic_type` / `extensions.animation` | WS-10 | After shared reader exists |
| Anim state / transition bound graphs | WS-10 mapping on shared walk | Read-only agent surface |
| Control Rig / RigVM | WS-10 compose Epic tools | **Not** UEdGraph — out of this ask |

## What breaks without it

- Full `animation.read_anim_bp` cannot emit ADR-0004 `nodes[]`/`links[]`.
- Agents stay on inventory-only responses (`partially_completed`,
  `fidelity.nodes_emitted=false`).
- Risk of WS-10 forking a second EdGraph walker that drifts from Blueprint hashing /
  identity rules.

Nothing protocol-blocking for montage inspection or AnimBP **inventory**.

## What I am doing meanwhile

WS-10 already ships inventory-only `read_anim_bp`:

- Graph name / class / `graph_type` / `node_count` / fidelity flags
- Nested state-machine discovery via `GetAllGraphs` + `SubGraphs`
- Semantic revision without emitting engine `GraphGuid`
- Honest `partially_completed`; **no** `asset_state` while ADR-0011 is Proposed
- Offline fixtures for classification, GUID churn, node-count revision, montage
  isolation

WS-10 will **not**:

- Promise AnimBP state-machine authoring
- Emit fake complete graphs
- Depend on Blueprint DSL for AnimGraph / state machines
- Edit `UeremcpBlueprint/**` or `schemas/graph/**`

## Suggested sequencing

1. WS-06 P0 lands (echo / stubs) — no change required for this ask.
2. WS-06 P1 shared `ReadEdGraph` with Blueprint EventGraph as first consumer.
3. WS-10 consumes reader for AnimBP EventGraph first (lowest semantic novelty).
4. WS-10 adds AnimGraph / AnimStateMachine semantic mapping +
   `extensions.animation` (domain-owned schema slice under
   `schemas/domains/animation/` only; envelope/`graph.schema.json` unchanged).
5. Keep `round_trip_supported=false` until a retrieve→replace→retrieve proof exists
   (authoring may remain permanently unsupported for state machines).

## Response

_(WS-06: accept Option A/B, propose alternate placement, or defer with a date.)_
