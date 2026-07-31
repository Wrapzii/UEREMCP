# WS-11 proposal — world document model, router plan output, primitive floor

**Raised by:** WS-11 (Validation & Testing)
**Date:** 2026-07-31
**Rationale:** Part VIII of `COVERAGE_PLAN.md` as written on
`ws-11-editor-gate-runtime-followup` (commit `dcdddb1`). **That branch's
COVERAGE_PLAN diverges from main's** — see "Divergence" at the end.
**Reference implementation:** `tools/world_doc_prototype.py`
**Conformance suite (built, 43 tests green):** `tests/world_doc/test_reconcile.py`

WS-11 owns `tests/**` and `UeremcpValidation/**`. Everything below is outside
that boundary, so it is proposed rather than edited (AGENTS.md rule 3). Item 8
of Part VIII.8 is done and lives in `tests/world_doc/`.

---

## 1. `schemas/world/world_doc.schema.json` + `read_world` / `apply_world`
**Owner: WS-05 (envelope) + Environment (semantics)**

Creation is currently verbs; verbs do not round-trip. Blueprints already prove
the alternative — `ReadGraph`/`SubmitGraph` over a `graph` document
[VERIFIED: schemas/domains/blueprints/submit_graph.schema.json]. Apply the same
model to the world.

Sections, in dependency order (`assets`, `terrain`, `water`, `structures`,
`foliage`, `weather`, `lighting`). List sections are keyed by stable `id` so the
diff is by identity, not by position. Full working shape in the prototype.

Response contract for `apply_world`:

```json
{
  "status": "completed | partially_completed | blocked",
  "base_revision": "sha256:...",
  "target_revision": "sha256:...",
  "plan": { "...": "conforms to schemas/batch/plan.schema.json" },
  "unresolved_dependencies": [],
  "refused": []
}
```

**The reconciler must be a pure function of `(actual, desired)`** — no ranking,
no model judgement. `tests/world_doc` asserts determinism, non-mutation of
inputs, backward-only `depends_on`, unique operation ids, and no-op on an
already-matching world.

## 2. Rewrite the `plan.schema.json` worked example
**Owner: WS-05**

The example at `schemas/batch/plan.schema.json:106` uses
`purpose: "fireball_core"` and a `features` list. Both resolve through
`templates/elements/` (see §5). From an empty project it produces nothing, and
it is the example every agent will copy. Replace with a chain that starts at
procedural textures and material-graph authoring, so the canonical demonstration
of batching is also a demonstration of building from scratch.

## 3. `resolve_intent` emits an `execute_plan` skeleton
**Owner: router owner (unassigned)**

`route_prototype.py::plan()` returns N individually-callable tools and never
mentions that `ExecutePlan` exists — though
`UeremcpCore.UeremcpReferenceToolset.ExecutePlan` is live
[VERIFIED: tools/registry_snapshot.json]. The batching engine in
`schemas/batch/plan.schema.json` is complete and unused because nothing ever
advertises it.

Return operations with `depends_on` pre-wired and `specification` blanks. Same
registry underneath; different output shape. Measured motivation: on the castle
intent the current prototype returns `confidence: low` and one step,
`retarget_node_class`, matched on the word "cycles."

## 4. `routing_advisory` on the response envelope
**Owner: WS-05**

Optional response field attached to any *successful* direct call to a
document-backed action. Escalates `info` → `warn` → `strong` with the number of
distinct domains called separately; carries `observed_round_trips` and
`would_have_been`.

**Advisory, never blocking.** Failing a legitimate single call teaches the agent
the tool is broken, which costs more than the round trips it saves. Semantics
are implemented in `Advisor` and asserted by `TestAdvisory`, including that
severity is monotonic and never `error`.

## 4b. Dependency cascade to the primitive floor — IMPLEMENTED
**Reference: `expand_to_floor` / `ASSET_RECIPES` in `tools/world_doc_prototype.py`**

Reporting *"you need a conifer mesh"* is half an answer. From an empty project
there is no mesh, no material for it, and no texture for that. Stopping at
`unresolved_dependencies` makes the agent discover the next missing rung one
round trip at a time — the exact cost model this project exists to remove.

A declared need now expands **recursively** until every leaf is something the
engine can build from parameters alone, and the leaves are emitted **first**.
Measured on the castle intent:

```
tex_foliage_conifer_surface_base_color   create_procedural_texture  yes  (floor)
tex_foliage_conifer_surface_normal       create_procedural_texture  yes  (floor)
mat_foliage_conifer_surface              submit_material_graph      NO   <- textures
mesh_foliage_conifer                     submit_mesh_ops            NO   <- material
tex_structure_stone_wall_base_color      create_procedural_texture  yes  (floor)
tex_structure_stone_wall_normal          create_procedural_texture  yes  (floor)
mat_structure_stone_wall                 submit_material_graph      NO   <- textures
terrain_terrain                          create_landscape           yes  (nothing)
structures_curtain                       place_structures           yes  <- material, terrain
foliage_conifers                         scatter_foliage            yes  <- mesh, terrain
```

Three properties this must keep, all asserted in `tests/world_doc`:

1. **Floor specs are parametric, never named.** `{"generate": "noise",
   "dimensions": [1024,1024]}`, not `"bark_preset"`. A name would reintroduce
   the library dependency the cascade exists to remove.
2. **Dedupe by role across the whole cascade.** Two structures needing the same
   stone material produce one material op and one shared pair of textures.
3. **Dependencies are precise, not rank-wide.** `terrain` depends on nothing;
   foliage depends on its own mesh, not on the wall material. Over-broad
   `depends_on` serialises work the executor could parallelise and misreports
   the graph. (Caught as a real defect during implementation.)

Rungs that cannot execute are named once in `blocked_rungs`, with the roles they
block and a source tag — so the plan shows the **correct shape** and states
exactly where it stops, rather than hiding the cascade behind "unresolved".

## 5. `submit_material_graph` — closes the material floor
**Owner: Material domain**

`create_vfx_material.purpose` is typed as a free string but its description
reads *"Semantic role selecting master template"*, and `master_template` reads
*"When omitted, resolved from purpose + element via WS-15 element templates
(`templates/elements/`)"*
[VERIFIED: schemas/domains/materials/create_vfx_material.schema.json].

**Open at the type level, closed at runtime by a library that must never ship.**
There is no action that authors a master material; everything instances from
one. A master material is a node graph, so this is `graph.schema.json` applied
to material expressions — the same document model, a different domain.

## 6. `submit_mesh_ops` — closes the mesh floor
**Owner: Mesh domain (requires GeometryScript enabled)**

No GeometryScript surface exists at all. Proposed shape: an op document
(primitive → extrude → boolean → displace → normals → collision). This is what
`castle_keep` and `foliage.conifer` actually need. The cascade (§4b) already
emits `submit_mesh_ops` in the right position with its material and textures
wired beneath it; the operation simply cannot execute until this exists.

## 7. Wire `UeremcpSecurity` for `PromoteToTemplate`
**Owner: WS-15** — unchanged from Part VII.4, restated because it is what closes
the loop. With §5 and §6 the agent can author from math and geometry ops;
promotion is what lets it *bank* the result. Per project direction the MCP ships
no library — the agent builds its own, per game.

---

## Cross-cutting rule proposed for CI

**An advertised enum value the implementation cannot honour is REFUSED, never
substituted.** A catalog entry is a contract. Now verified on main, and worse than reported:

| Claim | Evidence |
|---|---|
| Advertises three body types | `UeremcpEnvironmentToolset.h:56` — *"Create a WaterBody river/lake/ocean from spline points."* |
| Implements one | zero `AWaterBodyLake` / `AWaterBodyOcean` symbols in `UeremcpEnvironment` |
| Schema ships the gap | `build_environment.schema.json:73-74` — `lake` and `ocean` described only as *"Phase 2"* |
| Cubes-for-trees still live | `UeremcpEnvironmentService.cpp:1495-1497`, and `UeremcpWeatherFollower.cpp:33` loads a cube too |

This is the cubes-for-trees defect relocated to the schema layer, where it is
harder to catch because the call succeeds and the response shape is correct.

**`place_structures` already does this correctly** and should be the model:
`UeremcpEnvironmentService.cpp:795` rejects an unknown kind with
*"structure kind '%s' unsupported; use ice_wall_ring|barrier_wall|box_along_river"*
rather than substituting one. Refusal is already in the codebase; it is just
not applied consistently.

`TestRefusalNotDowngrade` asserts this against the reconciler: a refused entity
must produce **no** operation, must carry a reason and a source tag, and must
degrade `status` away from `completed`.

## Open ownership questions

- `tools/world_doc_prototype.py` and `tools/route_prototype.py` report as
  unowned (`check_ownership.py`: *"belongs to WS-01 until assigned"*). Requesting
  assignment rather than squatting.
- `docs/COVERAGE_PLAN.md` is likewise outside the WS-11 boundary. It was
  authored and committed by WS-11 at user direction; flagging for reassignment.


---

## Divergence: `docs/COVERAGE_PLAN.md`

`main` carries a 734-line `COVERAGE_PLAN.md` with Parts II–IV, ending in a
*"Part IV — Implementation ledger (2026-07-30 backlog integration)"*.

WS-11 authored a 1740-line version with Parts I–VIII (commit `dcdddb1` on the
now-deleted `ws-11-editor-gate-runtime-followup`). Its Parts V–VIII —
domain implementation detail, the subsystem boundary audit, the
script-vs-subsystem audit, and the world-document model — **are not on main.**

Not merged here, because the two documents diverge structurally and
`docs/` is outside the WS-11 boundary. Both versions exist in git; this needs an
owner decision, not a silent reconciliation. The substantive content of Part VIII
is reproduced in this proposal.
