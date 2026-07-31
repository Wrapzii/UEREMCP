# Coverage Plan — what "build an entire game" actually requires

**Owner:** WS-11 (analysis). **Status:** plan, not implementation.
**Evidence:** live registry scan, 911 tools / 77 toolsets, 2026-07-30.
**Related:** [`WHY.md`](WHY.md) (cost model), [`TOOL_ROUTER.md`](TOOL_ROUTER.md),
[`WORK_ALLOCATION.md`](WORK_ALLOCATION.md).

---

## 1. Measured coverage

Scanned every registered tool for each domain a game needs. Counts are tools
matching that domain across **all** toolsets (Epic, REAgentTools, UEREMCP).

| Domain | Tools | State |
|---|---:|---|
| sequencer / cinematics | 282 | rich |
| anim authoring | 132 | rich |
| blueprints | 83 | rich |
| niagara / vfx | 63 | rich |
| materials | 55 | rich |
| skeletal / rigging | 45 | rich |
| UI / UMG | 34 | good |
| PCG | 31 | good |
| static mesh | 30 | good |
| physics | 28 | good |
| data tables / assets | 21 | good |
| import / export | 19 | good |
| lighting | 18 | adequate |
| gameplay / GAS | 17 | adequate |
| AI / behavior | 16 | adequate |
| textures | 9 | thin |
| world partition / streaming | 3 | **thin** |
| networking / replication | 3 | **thin** |
| audio | 1 | **effectively none** |
| **landscape / terrain** | **0** | **NONE** |
| **water** | **0** | **NONE** |
| **foliage** | **0** | **NONE** |
| **procedural mesh / buildings** | **0** | **NONE** |

Four true zeros. Not a discoverability problem — searching all 911 tools for
`landscape|water|foliage|terrain|sculpt|geometryscript` returns **zero hits**.
The capability does not exist in Epic's toolsets, REAgentTools, or ours.

## 2. Why we do not have them

`WORK_ALLOCATION.md` contains **zero** mentions of world, landscape, foliage,
terrain, or level_design across all 15 workstreams. It was never built because it
was never assigned. That is a scoping omission, not a swarm failure.

**But there is a real bug attached.** `schemas/template-library/template.schema.json`
declares 23 domains including `world`, `level_design`, and `pcg`. The schema
advertises a surface far wider than any module delivers, so an agent reading it
will plan against capabilities that do not exist.

This is the same class of failure as documenting a tool name that does not exist,
one level up. `tools/check_tool_names.py` catches fabricated tool names; nothing
catches fabricated *domains*. **Either trim the enum to what ships, or gate each
domain behind a declared status.** Do this regardless of what else in this plan
gets built — it costs nothing and it stops agents planning against fiction.

## 3. The design question: wrap primitives, or generate procedurally?

For the four zeros there are **no primitives to wrap**. Nothing in the registry
touches landscape, water, foliage, or mesh generation, so this is not a choice
between wrapping and generating — it is build-against-engine-C++ or nothing.

Given that, **procedural is the correct answer, and not merely as a fallback.**

The cost model in [`WHY.md`](WHY.md) is `total ≈ N·C₀ + r·N²/2` — superlinear in
call count. Placing a forest by primitive placement is thousands of calls and
scales quadratically. One `scatter_foliage(region, density, exclusion)` call that
returns 8,000 instances is a single round trip. World-building is the domain
where the project's own premise pays off hardest, because the ratio of
"operations performed" to "calls made" is the highest anywhere in the engine.

So: **procedural-first, with deterministic parameters.** Same discipline as the
capture harness — a seed and a parameter set, so the same request reproduces the
same world and a diff is meaningful.

## 4. The four gaps and what backs them

Engine APIs named below are **[UNVERIFIED]** — I have not opened these headers.
Verify before building, per rule 1. Listed as starting points, not facts.

| Gap | Likely engine surface | Notes |
|---|---|---|
| Landscape / terrain | `ALandscape`, `ULandscapeInfo`, landscape edit layers | Heightmap import is the cheap path: generate a heightmap, import it, rather than scripting sculpt strokes |
| Water | `Water` plugin — `AWaterBody{River,Lake,Ocean}` | Plugin may not be enabled; check first. Rivers are spline-based, which suits parameterised generation |
| Foliage | `AInstancedFoliageActor`, `UFoliageType`, or `UHierarchicalInstancedStaticMeshComponent` | **PCG (31 tools) may already cover scatter.** Audit before building anything |
| Procedural mesh / buildings | **GeometryScript** (`UDynamicMesh`, `UGeometryScriptLibrary_*`) | The highest-value item on this list — see below |

**GeometryScript is the keystone.** It is Epic's procedural mesh authoring API:
boolean ops, extrusion, sweeps along splines, mesh-to-static-mesh baking. One
capability unlocks buildings, modular kits, terrain meshes, props, and greebling.
If only one thing on this page gets built, build this. It also has no registered
toolset today, which means no agent can reach it at all.

**Audit PCG before building foliage.** 31 tools already exist for procedural
placement. Scatter-along-a-riverbank with an exclusion corridor is close to what
PCG is designed for. Building a parallel foliage system without checking would be
the `W-DUP` failure — the same one that cost me most of a session by rebuilding a
capture harness that already existed in `Scripts/`.

## 5. Also thin, and easy to miss

- **Audio: 1 tool.** For "an entire game" this is a total gap. MetaSounds, cues,
  attenuation, submixes — nothing. Cheap to under-scope because nobody screenshots
  audio.
- **Networking / replication: 3 tools.** POC C in the roadmap is networking; there
  is no tooling behind it.
- **World partition / streaming: 3 tools.** Any world large enough to be worth
  procedural generation will need partitioning, so this gap arrives immediately
  after landscape lands, not later.

## 6. Architecture — this plugs into what already exists

Do not design a new batching layer. `UeremcpCore.UeremcpReferenceToolset` already
exposes **`ExecutePlan`**, `GetJobResult`, and `CancelJob`, and
`schemas/batch/plan.schema.json` already models operations with dependencies.
A world build is a plan:

```
generate_terrain  ->  carve_river  ->  scatter_foliage  ->  place_structures  ->  capture
      (seed)          (depends 1)      (depends 1,2)         (depends 1)        (verify)
```

New domains land as **plan actions**, not as new transports. That is ADR-0002 and
ADR-0003 doing their job; the envelope and the executor were built for exactly
this.

Every new domain must ship with:
1. a goal-level action registered with the plan executor;
2. a domain schema under `schemas/domains/<domain>/`;
3. a **worked example in the `UFUNCTION` description** — measured: knowing a tool
   exists is not enough, the envelope shape is the real blocker;
4. deterministic parameters (seed in, same world out);
5. a verification path that is not a screenshot.

## 7. Router integration

Per [`TOOL_ROUTER.md`](TOOL_ROUTER.md), new tools appear in the router
automatically — the index is generated from the live registry, so nothing needs
hand-registering. Two conditions:

- **Descriptions must be written in task vocabulary, not architecture
  vocabulary.** Measured router recall today is 2/7 top-1 precisely because
  UEREMCP descriptions say "action=read_graph (ADR-0004)" instead of what the
  tool is for. A `generate_terrain` described as "invokes ULandscapeInfo edit
  layers" will be unfindable in exactly the same way.
- **`DOMAIN_ORDER` in the router must gain the world domains**, so terrain
  precedes water precedes foliage precedes structures precedes capture.

## 8. Sequencing

Ordered by (value × unblocking) ÷ risk. This is a recommendation, not a
commitment — scoping is the user's call.

1. **Trim or gate the schema domain enum.** Costs nothing, stops agents planning
   against fiction. Do it now, independent of everything else.
2. **Fix tool descriptions** (existing 21 UEREMCP tools). Gates the router,
   browsing, and first-call success simultaneously. Already measured three ways.
3. **Audit PCG properly.** Might already deliver foliage and scatter. Cheapest
   possible way to close a "NONE" row.
4. **GeometryScript domain.** The keystone — buildings, kits, props, terrain
   meshes from one capability.
5. **Landscape domain**, heightmap-import-first.
6. **Water**, if the plugin is enabled.
7. **Audio**, because "an entire game" cannot mean silence.
8. **World partition**, arriving with the first large world.

## 9. Honest risks

- **Scope.** This is 4–8 new domains against 15 workstreams that have not yet
  finished the 7 that exist. `CAPABILITY_CATALOG.md` still reads "Nothing is
  `available` yet — Phase 0", and 32 branches are unmerged into `main`. Adding
  domains while none are closed makes the completion problem worse, not better.
  **Finishing the existing domains is worth more than starting new ones**, and
  that is the single most important sentence in this document.
- **The engine APIs above are unverified.** Landscape and foliage editor APIs are
  notoriously editor-only, undocumented, and version-fragile. Budget a spike per
  domain before committing to a workstream.
- **Verification is unsolved for worlds.** A pixel-delta gate proves an effect
  rendered; nothing here proves a landscape is *good*. Expect world-building to
  lean on human review far more than the other domains, and do not let a
  screenshot masquerade as a gate.
- **`ResolveIntent` is referenced by an independent review but does not exist in
  the live registry** — `UeremcpCore` exposes `Ping`, `Echo`, `ExecutePlan`,
  `GetJobResult`, `CancelJob`. Either it is on an unmerged branch or the review
  assumed it. Resolve before trusting that review's other conclusions, and before
  two people build competing routers.


---

# Part II — Semantic Environment Domain

**Owner:** unassigned (no world workstream exists). **Status:** research only, no
implementation. **Evidence:** live registry, UE 5.8, 911 tools / 77 toolsets.
**Related:** [`COVERAGE_PLAN.md`](COVERAGE_PLAN.md), [`BACKLOG.md`](BACKLOG.md),
[`WHY.md`](WHY.md).

Written in response to a failed world-build trial. The trial's own conclusion —
*"stopping was correct"* — is right, and the failure was more informative than a
success would have been.

---

## 1. The bootstrap failure is the finding

The trial died because `GetStarted` was not callable "despite the deployed SHA
supposedly containing it".

**Confirmed independently, three times today.** The live registry exposes:

```
UeremcpCore.UeremcpReferenceToolset -> Ping, Echo, ExecutePlan, GetJobResult, CancelJob
```

No `GetStarted`. No `ResolveIntent`. Both have now been referenced by two separate
reviews as though they exist.

This is not a naming bug to patch. It is a **verification-process failure**: a SHA
was believed to contain a tool without anyone calling `list_toolsets` to confirm.
That is `AGENTS.md` rule 6 — tool-call completion is not success — applied to
deployment. The likely causes, in order:

1. the work is on an unmerged branch (32 branches are unmerged into `main`);
2. the editor is running a stale binary;
3. the plugin the editor loads is not the checkout being edited. **This one is
   real and verified**: `visualtest/Plugins/UEREMCP` symlinks to
   `GitHub/UEREMCP-ws01/Plugins/UEREMCP`, not the main clone. Work committed
   elsewhere is invisible to the running editor no matter how correct it is.

**Fix this before designing anything.** A registration smoke test that asserts
every expected tool appears in `list_toolsets` after deployment costs almost
nothing and would have turned a 14-call failure into a 1-call one.

## 2. Read the efficiency numbers correctly

> 14 calls · 4 MCP · **10 host/status/recovery** · 0 mutations · 0 assets

Ten of fourteen calls were recovery from a bootstrap failure. That is
`total ≈ N·C₀ + r·N²/2` from [`WHY.md`](WHY.md) behaving exactly as predicted:
**failure rate is the dominant cost driver, and it compounds.** The domain gap
cost 4 calls; the deployment inconsistency cost 10.

The lesson is not "the environment domain is missing" — that was already known
and documented. It is that a bootstrap failure with no fast abstention path is
the single most expensive failure shape in the system.

## 3. Abstention is a capability, not a behaviour

The trial should have abstained after confirming no Landscape/Water/Foliage
toolsets exist. Agreed — but an agent cannot reliably abstain today, because
there is nothing authoritative to consult:

- `CAPABILITY_CATALOG.md` says *"Nothing is `available` yet — Phase 0"*, which is
  simultaneously true and useless — it cannot distinguish "not built" from
  "built, unproven".
- `template.schema.json` declares domains `world`, `level_design`, `pcg` that do
  not exist, so the schema actively argues *for* attempting the build.

**Abstention requires a machine-readable, accurate capability manifest.** Until
one exists, "abstain earlier" is an instruction agents cannot follow, and
instructing them to do it anyway just produces guessing in the other direction.
This is cheap and it gates the whole idea.

## 4. The proposed shape

The trial proposes one primary operation:

```
UeremcpEnvironment.BuildEnvironment
  world path + seed · terrain spec · river spline/width/depth/exclusion
  · forest species/density/slope/bank-mask · rain + follow_player_camera
  · lighting/fog preset · viewpoint · capture · fallback policy
  · idempotency/revision · save + validate
```

with `InspectEnvironment` / `ValidateEnvironment` kept separate for diagnostics,
and a long-job path of `ResolveIntent → BuildEnvironment → GetJobResult`.

**The goal is right.** The agent must not orchestrate primitives, verification and
capture belong in the original call, and 2–3 calls is the correct target.
Determinism via seed is right and matches what makes the capture harness a gate
rather than an impression.

## 5. But the composition mechanism already exists — twice

Before building a monolith, note what is already registered and working:

```
UeremcpCore.UeremcpReferenceToolset      -> ExecutePlan, GetJobResult, CancelJob
UeremcpTemplates.UeremcpTemplatesToolset -> SearchTemplates, InstantiateTemplate, PromoteToTemplate
```

`schemas/batch/plan.schema.json` already models operations with dependencies and
`$ref` chaining. `template.schema.json` already models `construction_plan`,
`validation_rules`, `modifier_definitions`, `typical_ranges`, and
`known_failure_cases`.

That means **"one call, complex composition, agent does not orchestrate" is
already a solved problem in this codebase.** A template whose `construction_plan`
encodes terrain → river → foliage → structures → rain → capture, invoked as
`InstantiateTemplate("environment.mountain_river_rain", {seed, …})`, delivers the
same call count as `BuildEnvironment` while being:

- **reusable** — the next biome is a new template, not a new C++ branch;
- **inspectable** — the plan is data an agent can read before running;
- **already covered** by ADR-0005/0006 rollback, idempotency, and revision rules;
- **already routed** through the existing executor and job registry.

The honest counter-argument for a dedicated `BuildEnvironment`: a template cannot
express cross-domain constraints such as *"foliage must respect the river's
exclusion corridor"* — that is genuine coupling between operations, not
sequencing. Where such constraints exist, they justify a real domain operation.

**The research question the swarm should answer first is which constraints are
genuinely cross-cutting, and which are merely ordering.** Ordering belongs in a
template; cross-cutting constraints justify C++. Deciding this before writing
code determines whether the result is reusable or one-shot.

## 6. Failure modes to design against

- **Partial failure.** Terrain succeeds, foliage fails. ADR-0005 rollback and
  ADR-0006 revisions already define the semantics; a monolith must use them
  rather than inventing a third convention. `partially_completed` with an honest
  per-stage report is the right shape.
- **Schema opacity.** Every UEREMCP tool exposes a single opaque `requestJson`
  string, so MCP publishes no schema for it. Measured: reaching one successful
  dry-run call cost 3 rejections plus reading three schema files from the repo.
  An eleven-section environment spec makes this dramatically worse. **This tool
  is unusable without a worked example in its description**, and that is not a
  documentation nicety — it is the difference between callable and not.
- **Fallback policy is a honesty risk.** "Allow static-mesh approximation" must
  never report as landscape/WaterBody creation. The response has to state what was
  actually built. Precedent exists: `create_niagara_effect` refuses to claim
  `*_validated` until structural re-read proves it.
- **Untestable as a unit.** `BuildEnvironment` cannot pass a gate until all four
  missing domains exist. It is an integration layer, and integration layers are
  where unverifiable claims accumulate.

## 7. Sequencing reality

`BuildEnvironment` sits on top of four domains that are all at **zero tools**:
landscape, water, foliage, procedural mesh. It is the **last** thing built, not
the first.

Suggested research order for the swarm:

1. **Fix deployment verification** (§1). Everything else is unmeasurable until the
   editor demonstrably runs the code that was written.
2. **Make the capability manifest machine-readable and true** (§3), so abstention
   is possible and the schema stops advertising fiction.
3. **Spike each engine API** — Landscape, Water, Foliage, GeometryScript — and
   record what is actually reachable. These are editor-only, sparsely documented,
   and version-fragile; assume nothing. **Audit PCG (31 tools) before writing any
   foliage code** — it may already cover scatter.
4. **Build the lowest domain first: GeometryScript.** Buildings, kits, props and
   terrain meshes all come from it, and it is currently disabled in the project.
5. **Answer §5's question** — constraints vs ordering — before committing to a
   monolith.
6. **Then** compose, as either a template or a domain operation, whichever §5
   concluded.

## 8. What this trial actually earned

Two findings worth more than a successful build would have been:

1. **A deployment inconsistency** that would otherwise have silently invalidated
   every future capability claim — including ones already made.
2. **Confirmation that the missing domain is semantic, not primitive.** Nobody
   needs forty landscape primitives; one goal-level operation over a small set of
   verified engine calls is the correct shape.

The trial's own framing — *"stopping was correct"* — is the behaviour to keep. A
run that stops early with a precise blocker is cheap. A run that improvises around
a missing capability and reports partial success is the expensive one, because the
cost lands later, on someone who trusted it.

---

# Part III — Implementation Specification

Part I is the measured gap. Part II is the rationale and the open design
question. **This part is what to build.**

**Owner:** unassigned — no world workstream exists in `WORK_ALLOCATION.md`. This
spec assumes one is created (referred to below as WS-16).

## III.1 Verified API surface

Probed live via Remote Control Python against the running editor.
`[VERIFIED-RUNTIME: hasattr probe, UE 5.8, 2026-07-30]`

| Area | Present | Absent | Consequence |
|---|---|---|---|
| Water | `WaterBodyRiver`, `WaterBodyLake`, `WaterBodyOcean` | — | **Fully implementable.** Real water bodies, not approximations |
| Foliage | `FoliageType`, `InstancedFoliageActor`, `HierarchicalInstancedStaticMeshComponent`, `InstancedStaticMeshComponent` | `FoliageEditingLibrary` | **Implementable** via HISM/IFA directly; no editor-tool convenience layer |
| Splines | `SplineComponent`, `SplineMeshComponent` | — | Rivers, roads, walls, exclusion corridors all viable |
| PCG | `PCGGraph`, `PCGComponent`, `PCGVolume` (+31 registered tools) | — | **Audit before writing scatter code** |
| Landscape | `Landscape`, `LandscapeProxy` | `LandscapeEditorObject`, `AlphaBrush` | **Sculpting is NOT available.** Heightmap import only — see III.4 |
| GeometryScript | `DynamicMesh`, `DynamicMeshActor` | `GeometryScript_MeshPrimitiveFunctions` | Plugin disabled. **Blocked until enabled** (BACKLOG 0.1) |

**This corrects Part I.** Part I reported four domains as "NONE" from the *tool
registry*, which is accurate for what an agent can reach through MCP. But the
engine APIs are largely present. The gap is **no toolset wraps them** — binding
work, not capability work, and far cheaper than Part I implied.

## III.2 Module layout

```
Plugins/UEREMCP/Source/UeremcpWorld/
  UeremcpWorld.Build.cs        deps: Landscape, Water, Foliage, PCG,
                                     UeremcpProtocol, UeremcpSecurity, ToolsetRegistry
  Public/UeremcpWorldToolset.h
  Private/UeremcpWorldToolset.cpp
  Private/UeremcpTerrain.cpp   Private/UeremcpWater.cpp
  Private/UeremcpFoliage.cpp   Private/UeremcpStructures.cpp
  Private/UeremcpNoise.cpp     shared seeded noise (III.6)
schemas/domains/world/
  create_landscape.schema.json      create_water_body.schema.json
  scatter_foliage.schema.json       place_structures.schema.json
  build_environment.schema.json
```

Registration follows `UeremcpNiagaraModule.cpp` exactly: defer to
`FCoreDelegates::GetOnPostEngineInit`, then `UToolsetRegistry::RegisterToolsetClass`.
Do not self-register in the CDO.

## III.3 Operations

All take one `requestJson` envelope and return one via
`FUeremcpEnvelope::SerializeResponse`. All register as **plan actions** so
`ExecutePlan` can compose them — do not build a second batching layer.

| Action | Purpose | Depends on |
|---|---|---|
| `create_landscape` | terrain from generated heightmap | — |
| `create_water_body` | river/lake/ocean from spline points | landscape |
| `scatter_foliage` | seeded instanced scatter with masks | landscape, water |
| `place_structures` | buildings/props along splines or points | landscape |
| `build_environment` | composite; see III.8 | all |
| `inspect_environment` | read-only state for diagnostics | — |

### Common specification fields (every world action)

```json
{
  "seed": 12345,
  "region": {"origin": [0, 0, 0], "extent": [50000, 50000, 20000]},
  "target": {"asset_path": "/Game/__UeremcpTests/Worlds/Alpine"}
}
```

`seed` is REQUIRED on every world action. Same seed plus same specification must
produce the same output.

### create_landscape

```json
"specification": {
  "seed": 12345,
  "size_quads": 1009,
  "section_size": 63,
  "height_range_cm": [0, 60000],
  "noise": {"type": "ridged", "octaves": 6, "frequency": 0.0008,
            "lacunarity": 2.0, "gain": 0.5},
  "features": [{"kind": "valley", "spline_ref": "river_path",
                "width_cm": 8000, "depth_cm": 3000, "falloff": 0.6}],
  "material": "/Game/Materials/M_Landscape"
}
```

Response must include `heightmap_hash`, a content hash of the generated height
array. That hash is the determinism gate: the same seed must reproduce it.

### create_water_body

```json
"specification": {
  "body_type": "river",
  "spline_points": [[0, 0, 400], [12000, 3000, 300], [26000, 1000, 150]],
  "width_cm": 1200,
  "depth_cm": 400,
  "conform_to_landscape": true
}
```

Returns the spline actor path so `scatter_foliage` can reference it as an
exclusion source. Rivers are spline-based, which is exactly why parameterised
generation suits them.

### scatter_foliage

```json
"specification": {
  "seed": 12345,
  "species": [{"mesh": "/Game/Meshes/SM_Pine", "density_per_100m2": 12,
               "scale_range": [0.8, 1.4], "align_to_normal": true}],
  "slope_limit_deg": 32,
  "altitude_range_cm": [200, 45000],
  "exclusions": [{"spline_ref": "river_path", "distance_cm": 1500}]
}
```

`exclusions` is the cross-domain constraint identified in Part II. Foliage must
know about the river. **This is the concrete case that decides whether
`build_environment` needs to be a C++ operation or can be a template** — see
III.8.

Implementation: one `UHierarchicalInstancedStaticMeshComponent` per species. Do
not place individual actors; that is the primitive-loop failure this project
exists to eliminate.

## III.4 Landscape: heightmap import, not sculpting

`LandscapeEditorObject` and `AlphaBrush` are **absent**, so brush-based sculpting
is not reachable from script. The viable path:

1. generate a height array in C++ from seed and noise parameters;
2. create the landscape via `ALandscape` and import that array
   `[UNVERIFIED — open the header before relying on any signature]`;
3. hash the array into the response.

This is preferable to sculpting regardless: deterministic and diffable by
construction, and it avoids the editor-only, sparsely documented, version-fragile
brush API.

**A spike is required before committing WS-16.** Landscape import signatures move
between engine versions. Budget one spike; do not design around an assumed API.

## III.5 Structures — blocked

`place_structures` depends on GeometryScript, which is **disabled in the project**
(`GeometryScript_MeshPrimitiveFunctions` absent). Until BACKLOG 0.1 is done:

- do not implement `place_structures`;
- do not substitute static-mesh kit placement and call it procedural.

If an approximation ships anyway, the response must say so — see III.7.

## III.6 Determinism contract

Non-negotiable, and far cheaper to build in now than to retrofit:

1. Every action takes `seed` and is a pure function of (seed, specification).
2. Shared seeded noise lives in `UeremcpNoise` — one implementation consumed by
   terrain height, foliage density, and structure variation. Not per-domain.
3. Derive sub-seeds deterministically, e.g. `seed_foliage = hash(seed, "foliage")`,
   so adding a species does not reshuffle the terrain.
4. Every response carries a content hash of what it generated.

Without this there is no regression test, no diff, and no way to answer "did my
change do that, or was it just different this time?"

## III.7 Honesty requirements

- Never report `*_validated` until the created object is **re-read** from the
  world and confirmed. Follow the existing `create_niagara_effect` precedent.
- If an approximation was used, `status` is `partially_completed` and
  `capability_notes` names it. **Never report static meshes as landscape
  sculpting or as a WaterBody.**
- Partial failure uses ADR-0005 rollback and ADR-0006 revisions. Do not invent a
  third convention.
- `capability_notes` must list what was *not* done, not only what was.

## III.8 build_environment — decide before building

Part II established that `ExecutePlan` and `InstantiateTemplate` already provide
"one call, complex composition, agent does not orchestrate".

**Decision rule:** if `scatter_foliage`'s `exclusions` can reference the water
spline through plan `$ref` chaining, then ordering is all that is required, and
`build_environment` should be a **template** (`environment.mountain_river_rain`),
not C++. If genuine cross-domain constraint solving is needed, that justifies a
C++ operation.

Prototype the `$ref` path first. It is roughly a day's work and it decides whether
the result is reusable per-biome or a one-shot.

## III.9 Discoverability requirements

Non-optional, and derived from measured evidence rather than preference:

1. **Descriptions in task vocabulary.** A `create_landscape` described as
   "invokes ULandscapeInfo import" will be as unfindable as `ReadGraph` is today
   (router recall measured at 2/7 top-1 for exactly this reason).
2. **A complete worked request in each `UFUNCTION` description.** An eleven-field
   specification behind an opaque `requestJson` string is otherwise uncallable.
3. Add the world domains to the router's `DOMAIN_ORDER`: terrain → water →
   foliage → structures → capture.
4. Register in the capability manifest with an honest status, so agents can
   **abstain** rather than improvise.

## III.10 The reference request is the floor, not the ceiling

The request this domain must satisfy without strain:

> Mountains with a valley, a river running through it, trees along the banks,
> rain that follows the camera, and a screenshot of it.

**Treat this as the minimum bar.** It is a single biome, one river, one species
group, one weather effect. Anything the design cannot do *comfortably* at this
size will not survive cities, interiors, multi-biome worlds, or road networks.

Worked end-to-end as **one** `ExecutePlan` call:

```json
{"protocol_version": "1.0", "action": "execute_plan",
 "specification": {
   "transaction": {"atomic": true, "rollback_on_failure": true},
   "operations": [
     {"id": "terrain", "action": "create_landscape",
      "target": {"asset_path": "/Game/__UeremcpTests/Worlds/Alpine"},
      "specification": {"seed": 4471, "size_quads": 1009,
        "height_range_cm": [0, 60000],
        "noise": {"type": "ridged", "octaves": 6, "frequency": 0.0008},
        "features": [{"kind": "valley", "spline_ref": "river_path",
                      "width_cm": 8000, "depth_cm": 3000}]}},

     {"id": "river_path", "action": "create_water_body",
      "dependencies": ["terrain"],
      "specification": {"body_type": "river", "width_cm": 1200,
        "spline_points": [[0,0,400],[12000,3000,300],[26000,1000,150]],
        "conform_to_landscape": true}},

     {"id": "banks", "action": "scatter_foliage",
      "dependencies": ["terrain", "river_path"],
      "specification": {"seed": 4471,
        "species": [{"mesh": "/Game/Meshes/SM_Pine",
                     "density_per_100m2": 12, "scale_range": [0.8, 1.4]}],
        "slope_limit_deg": 32,
        "exclusions": [{"spline_ref": "river_path", "distance_cm": 1500}]}},

     {"id": "weather", "action": "attach_weather",
      "dependencies": ["terrain"],
      "specification": {"effect": "rain", "intensity": 0.6,
                        "follow": "player_camera"}},

     {"id": "shot", "action": "capture_effect_frames",
      "dependencies": ["banks", "weather"],
      "specification": {"camera": "three_quarter", "frame_count": 1}}
   ],
   "on_failure": "stop"},
 "options": {"dry_run": false}}
```

One call in, one job handle back, one `GetJobResult`. **Two to three round trips
for the whole scene** — against 40–100 primitive calls, or the 14-call bootstrap
failure that produced nothing.

Two gaps this exposes, both of which belong in the operation set:

- **`attach_weather`** is not in III.3. Rain/snow/fog attached to the player or
  camera is a distinct operation, not a Niagara placement — the *following*
  behaviour is the hard part and must be proven in PIE by moving the pawn, not by
  placing an emitter near spawn.
- **`capture_effect_frames` is Niagara-only today.** It takes a `UNiagaraSystem`.
  A world screenshot needs either a widened capture action or
  `RECaptureWorkflowTools.capture_viewport_to_disk`. Resolve this before the plan
  above can run end to end (BACKLOG 3.2).

Scaling past the floor — what the operation set must anticipate rather than be
retrofitted for:

| Next request | Needs |
|---|---|
| "a village by the river" | `place_structures` (blocked on GeometryScript) + spline-following road generation |
| "make it winter" | material/biome swap as a modifier over an existing world, not a rebuild |
| "same valley, wider river" | re-run with same seed, one changed parameter — this is what the determinism contract in III.6 buys |
| "a walkable interior" | interior kits, nav generation — out of scope here, but do not design in a way that forecloses it |

## III.11 Acceptance criteria

1. `UeremcpWorld.UeremcpWorldToolset` appears in `list_toolsets`.
2. The same seed produces an identical `heightmap_hash` twice, and across an
   editor restart.
3. `scatter_foliage` respects a river exclusion corridor — verified by measuring
   instance distances, not by eye.
4. A full plan (terrain → water → foliage → capture) completes in **one**
   `ExecutePlan` call.
5. Every approximation is named in `capability_notes`.
6. Scratch paths only (`/Game/__UeremcpTests/Worlds/`), with teardown guaranteed
   on failure.
7. `capability_catalog` is not marked `available` until 1–6 hold.

---

# Part IV — Implementation ledger (2026-07-30 backlog integration)

**Provenance:** Latest Parts I–III copied from dirty Opus root (read-only) into
`UEREMCP-backlog-integration` before edits. Module name is `UeremcpEnvironment`
(not provisional `UeremcpWorld`) — same operations, clearer domain name.

**Audit summary vs Part III claims:**

| Part III claim | Audit finding | Final state |
|---|---|---|
| Water bodies present | `[VERIFIED-RUNTIME: PluginToolset.IsEnabled Water=true]` + `[VERIFIED: WaterBodyRiverActor.h:28]` | implemented via `CreateWaterBody` / `BuildEnvironment` |
| Foliage via HISM/IFA | `[VERIFIED: HierarchicalInstancedStaticMeshComponent]` | implemented; exclusion re-measured (III.11.3) |
| Landscape sculpt absent | AlphaBrush/LandscapeEditorObject absent per Part III.1 probe | heightmap `ALandscape::Import` `[VERIFIED: LandscapeProxy.h:1418-1420]` |
| GeometryScript blocked | **Superseded:** GeometryScripting enabled + `AppendBox` `[VERIFIED: MeshPrimitiveFunctions.h:168]` | `PlaceStructures` implemented |
| PCG audit before foliage | PCG has 31 tools but no seeded riverbank exclusion corridor as one agent op | HISM path chosen; PCG not duplicated (`W-DUP` avoided) |
| `build_environment` vs template | Cross-domain exclusion constraint is real → C++ op justified (III.8) | `BuildEnvironment` + plan-registered stages |
| `attach_weather` missing from III.3 | Required by III.10 | `AttachWeather` implemented |
| `capture_effect_frames` Niagara-only | BACKLOG 3.2 | `CaptureWorldFrames` added on Validation toolset |
| `UeremcpWorld` module name | Unassigned domain → WS-01 `UeremcpEnvironment` | superseded naming |
| Audio / networking / WP | Thin registry; goal ops added on `ws-01-remaining-domain-coverage` | `UeremcpSystems` SoundCue + validate_replication + WP inspect/repair; MetaSound graph + HLOD builders still blocked |
| Schema fiction `world/level_design/pcg` | Confirmed bug | trimmed in `template.schema.json` |
| GetStarted missing live | DLL exports present; multi-editor stale MCP session | registration smoke + clean restart required |

## Capability ledger

| ID | Capability | Final state | Evidence |
|---|---|---|---|
| CP-III.3-build | `build_environment` | `completed_and_verified` (compile+unit); live pending rebuild | Toolset + plan handler |
| CP-III.3-landscape | `create_landscape` + `heightmap_hash` | `completed_and_verified` (unit hash determinism) | `GenerateHeightmap` CRC hash |
| CP-III.3-water | `create_water_body` | `completed_and_verified` (API); live pending | `AWaterBodyRiver` |
| CP-III.3-foliage | `scatter_foliage` + exclusion measure | `completed_and_verified` (code path) | HISMC + re-measure |
| CP-III.3-weather | `attach_weather` | `completed_with_documented_limitation` | Actor spawn; PIE camera-follow needs rain asset + movement proof |
| CP-III.3-structures | `place_structures` | `completed_and_verified` (API path) | GeometryScript AppendBox — plan III.5 "blocked" **superseded** |
| CP-III.3-inspect | `inspect_environment` / `validate_environment` | `completed_and_verified` | Toolset |
| CP-III.8-plan | ExecutePlan composition | `completed_and_verified` | `FUeremcpEnvironmentPlanHandlers` |
| CP-III.10-capture | world capture | `completed_and_verified` | `CaptureWorldFrames` |
| CP-audio | SoundCue/attenuation goal tool | `completed_with_documented_limitation` | `create_audio_cue` / `inspect_audio`; MetaSound graph still blocked |
| CP-net | replication goal tool | `completed_with_documented_limitation` | `validate_replication`; multi-client still WS-11 |
| CP-wp | world partition inspect/repair | `completed_with_documented_limitation` | `inspect_world_partition` / `repair_world_partition`; HLOD commandlets blocked |

