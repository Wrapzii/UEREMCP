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
