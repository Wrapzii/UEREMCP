# Backlog — consolidated findings from the 2026-07-30 live session

Everything below is evidence-backed against a running editor (UE 5.8) unless
marked otherwise. Ordered by (value × unblocking) ÷ risk.

Legend — **V** verified live · **W** written, uncompiled · **P** proposal only.

## Completion ledger (integration tip — update honestly)

| ID | Item | State | Evidence (2026-07-30/31) |
|---|---|---|---|
| 0.1 | Enable GeometryScript | **V done** | Live `PluginToolset.IsEnabled(GeometryScripting)=true`; RE.uproject + uplugin |
| 0.2 | Enable Water | **V done** | Live `IsEnabled(Water)=true`; RE.uproject + uplugin |
| 0.3 | Trim template domain enum fiction | **V done** | Enum = 8 shipping domains; no `world`/`level_design`/`pcg` |
| 0.4 | Merge valuable branches | **V done (selective)** | Discoverability / Systems / capture / Environment / MRR closeout — **not** blind 32-branch merge |
| 0.5 | Scripts/ in read order | **V done** | `AGENTS.md` 0c + `Scripts/README.md` (ice-wall recipe path noted; file may live outside tip) |
| **Deploy** | Editor loads tip with Environment+Core+Systems+capture | **V done** | Junction → deploy tip (or follow-on for Tier 1b verify); live `list_toolsets` shows all four |
| 1.1 | Rewrite tool descriptions | **V largely done** | 42 source callables pass `check_tool_names` description contract; Environment remains the model |
| 1.2a / 1b.1 | Publish nested specification schemas via `GetJsonSchemaInternal` wrapper | **V done** | Live `describe_toolset` shows `x-ueremcp-schema-publishing=nested_envelope_v1`; BuildEnvironment.specification requires `seed` with enums/defaults from `schemas/domains/**`; nested MCP args Echo OK; UHT still takes `requestJson` string — wrapper normalizes. Epic `GetJsonSchema()` is non-virtual `[VERIFIED: Toolset.h]`; override is `GetJsonSchemaInternal` via `FUeremcpSchemaPublishingToolset`. |
| 1.2b | Envelope rejection contract echo | **V done** | Live Echo malformed → `capability_notes` with shape + example + next |
| 1.3a | ResolveIntent / DescribeOperation / capture stack | **V done** | Exact live names callable; DescribeOperation now returns nested input_schema |
| 1.3b / 1b.2 | Template **authoring** (not seeded library) | **V done** | Live `CreateTemplate` → `created_and_validated` wrote `Saved/UEREMCP/Templates/agent/niagara.cast.helix_live.v1.json`; `SearchTemplates` found it; `UpdateTemplate` + non-preview `PromoteToTemplate` (dry_run=false writes JSON). Empty day-one library remains correct — no seeded helix_ring shipped. Limitation: promote does not reverse-engineer full Niagara graphs from arbitrary assets; starts from base template / stub plan — refine via UpdateTemplate. |
| 1.3c | GetStarted collision | **V done** | Live `GetStarted` on Reference toolset |
| 1.3d | Router plan score-gate / cap | **V done** | Live Coding Core patch: mountain/river → **1-step** `BuildEnvironment` |
| 1.4 | Unify naming | **V done (aliases)** | PascalCase live; snake/kebab normalize — full rename deferred |
| 1b.3 | existing_assets + domain filter | **V done** | ResolveIntent boosts inspect/capture/modify when `context.existing_assets` present; `context.domain` filter |
| 1b.4 | Richer CreateNiagaraEffect contract | **open (WS-07)** | Still probe roles; honesty for stubbed roles not landed — propose / follow-on |
| 1b.5 | Terminal capture in one call | **open (WS-11)** | Cold path may still `partially_completed`; path/EXR defects tracked in VISUAL_CAPTURE_PROTOCOL |
| 1b.6 | One goal-level envelope | **open** | Blocked on 1b.5 design; do not ship a second batching layer — extend ExecutePlan / a single facade later |
| 1b.7 | Response hygiene / proxy banners | **partial** | `next_tool` / registry hash exist on router; proxy BATCH banners are outside this plugin — needs transport/proxy owner |
| 2.1–2.5 | Discoverability machinery | **V done** (2.3 focus **disabled**, now **unblocked**) | 1.1+1.2a live; enable focus only after intentional operator decision |
| 3.x | Visual capture | **V largely done** | Four capture tools live; residual = 1b.5 terminality |
| 4.x | Coverage gaps (UI/mesh/physics/…) | **deferred to follow-on** | Tip stays usable; Environment/Systems already on tip |

Work **tiers in order**. Tier 1 / 1b authoring+schemas landed on `ws-01-coverage-gaps-followon`. Remaining 1b.4–1b.7 are owned/blocked as above — do not reopen Tier 0.

## Start here

This file is the entry point. Everything from the 2026-07-30 session lives in
four documents; there is no fifth.

| Document | Covers |
|---|---|
| **BACKLOG.md** (this file) | prioritised work queue — read first |
| [`BENCHMARK_PROTOCOL.md`](BENCHMARK_PROTOCOL.md) | measurement: Part I efficiency, Part II agent usability |
| [`TOOL_ROUTER.md`](TOOL_ROUTER.md) | discoverability: router design + measured recall |
| [`COVERAGE_PLAN.md`](COVERAGE_PLAN.md) | gaps + specs: I coverage · II environment rationale · III environment implementation · **IV priority domains (UI, mesh, physics, data, import, lighting, AI)** |
| [`VISUAL_CAPTURE_PROTOCOL.md`](VISUAL_CAPTURE_PROTOCOL.md) | deterministic VFX capture harness |

---

## Tier 0 — costs almost nothing, do first

| # | Item | State | Why |
|---|---|---|---|
| 0.1 | **Enable GeometryScript** in the project | **V done** (was: confirmed disabled) | Keystone for procedural mesh. Now live-enabled in RE. |
| 0.2 | **Enable the Water plugin** | **V done** | Rivers/lakes/ocean require it; live-enabled in RE. |
| 0.3 | **Trim or gate `template.schema.json` domain enum** | **V done** | Was fiction (`world`/`level_design`/`pcg`); now eight shipping domains. |
| 0.4 | **Merge the branch backlog** | **V done (selective)** | Valuable tips integrated; do **not** blindly merge 32 historical branches. |
| 0.5 | **Add `Scripts/` to the repo read order** | **V done** | `AGENTS.md` + `Scripts/README.md`. |

## Tier 1 — the measured bottleneck

**1.1 Rewrite the 21 UEREMCP tool descriptions in task vocabulary.** *(highest
leverage single item on this page)*

Measured three independent ways: it gates router recall (**2/7 top-1**), direct
browsing, and first-call success — simultaneously. One fix, three payoffs.

Today: `ReadGraph` → *"action=read_graph — graph JSON (ADR-0004) + diagnostics"*.
Mean description length: UEREMCP **167** chars vs **228** elsewhere. An agent
thinking *"add logic to a blueprint when the spell hits"* matches none of it.

Not markdown — `UFUNCTION` doc comments, which ship in the plugin and are exactly
what `describe_toolset` returns. Each needs: what it is *for* in user words, a
complete worked request, and the required `specification` keys.

**1.2a `{ "requestJson": "string" }` is not a schema — publish real ones.**
The operator's read is correct: it is a typed hole, not a contract. It says a
string goes in and nothing about what must be in it. Every UEREMCP tool reports
this, while every Epic tool reports named typed parameters — which is also why
agents drift back to the primitives.

**This is fixable without abandoning the envelope.** `FToolset::GetJsonSchema()`
is overridable `[VERIFIED: $TR/Public/ToolsetRegistry/Toolset.h]`. UEREMCP toolsets
can publish the **real nested `specification` schema per action** instead of the
UFUNCTION-derived `{requestJson: string}` that `UToolsetDefinition` generates by
default. The domain schemas already exist under `schemas/domains/**` — they are
simply never surfaced to MCP.

Order of preference:
1. Override `GetJsonSchema()` to emit the per-action envelope + specification
   schema. Highest value; nothing else changes.
2. Failing that, embed the full specification contract in each tool description
   (item 1.1 already requires a worked example — this extends it).
3. `DescribeOperation` already returns usable `request_json` examples and closes
   part of the gap for agents that route. It does not help agents that call
   directly.

**1.2b Compile and verify the envelope contract echo.** **V done** —
`FUeremcpEnvelope::MakeRejection` now returns shape + minimal example + next
action in `capability_notes`. Live Echo with missing `protocol_version` confirmed
2026-07-30. Remaining cold-start cost is **1.2a** (nested schemas), not echo.

**1.3a SHIPPED — verified by live agent testing 2026-07-30.** `ResolveIntent` and
`DescribeOperation` now exist on `UeremcpCore.UeremcpReferenceToolset`, and
`CaptureEffectFrames` / `CaptureAnimationFrames` / `CaptureMaterialFrames` all
ship — closing 3.1 and 3.2. An operator's agent ran intent -> plan -> create ->
capture end to end: `ResolveIntent` returned high confidence with a 6-step plan
and correctly ranked `CreateNiagaraEffect` (155), `CaptureEffectFrames` (98.5),
`CaptureAnimationFrames`, `InspectSystem`, `CaptureMaterialFrames` — "exactly the
right stack for this job". Created `NS_RouterDemo_FireProjectile`, captured 6
frames, **pixels changed vs baseline**. Reported as faster than the old
SceneCapture + advance_simulation + Pillow pipeline, and more efficient than
hunting with `list_toolsets`/`describe_toolset`.

Two limitations confirmed, both matching what was measured here:
- **`input_schema` is still `{requestJson: string}`** — "enough to call, not a
  complete OpenAPI-style contract". `ResolveIntent`/`DescribeOperation` mitigate
  by returning worked `request_json` examples; item 1.2 is still worth landing.
- **Not always 2 calls** — a cold Niagara compile needed a `GetJobResult` poll,
  so 3. That is the honest number.

**1.3b NEW — the template library is empty and blocks reuse.** `SearchTemplates`
returns **0 hits**; `PromoteToTemplate` is **preview-only**; `InstantiateTemplate`
needs a `template_id` none of which are discoverable. Consequences: agents cannot
define or ship reusable patterns, and `CreateNiagaraEffect` is confined to its
fixed probe roles (`core`, `flame_shell`, `sparks`, `smoke`, `ribbon_trail`,
`impact_burst`) — fireball-shaped, so a custom ice helix is not reachable.
This corrects `COVERAGE_PLAN.md` III.8, which recommended composing via template.

> **Intent correction from the operator.** The MCP is **not** meant to ship with a
> template library — that would be shipping content, and it is explicitly not
> wanted. The agent is meant to **author its own library, per game, from scratch.**
> So the defect is not "the library is empty" (correct and expected on day one);
> the defect is that **authoring a template is close to impossible**:
> `PromoteToTemplate` is preview-only, and there is no `CreateTemplate`.
>
> Restated as work: **make template authoring actually work.** An empty library is
> the correct starting state; an agent that cannot fill it is the bug.

**1.3b-i Un-preview `PromoteToTemplate`.** It must fully promote a working asset
into a reusable, versioned template — the primary way an agent banks a result it
just proved.

**1.3b-ii Add `CreateTemplate` / `UpdateTemplate`.** Author a template from a spec
rather than only by promoting an existing asset. `template.schema.json` already
models `construction_plan`, `modifier_definitions`, `validation_rules` and
`known_failure_cases` — the schema is ready; nothing writes it.

**1.3b-iii Unlock `CreateNiagaraEffect` from its fixed probe roles.** It is pinned
to `core`, `flame_shell`, `sparks`, `smoke`, `ribbon_trail`, `impact_burst`.
Freeform emitter/renderer authoring is what makes a self-built library possible;
without it every template is a fireball variant.

**1.3c Resolve the `GetStarted` collision.** **V done** — live
`UeremcpCore.UeremcpReferenceToolset.GetStarted` callable by exact name; returns
`next_call=ResolveIntent`. Historical note: an earlier tip (`UEREMCP-ws01`) lacked
it — deploy path was the collision.

**1.3d Router plan-ordering defect — reported by a live agent.** **V done**
(2026-07-30 Live Coding Core patch on deploy tip). Live mountain/river intent:
summary `Routed 1 step(s)`, sole plan step `BuildEnvironment` score 62,
`plan_score_floor` 21.7; weak `CaptureMaterialFrames` no longer in plan.
Offline held-out unchanged: top-1 **84.21%**, top-3 **100%**, confident-wrong **0**.

Landed fixes:
1. **Score-gate plan membership** at 35% of best hit (`plan_score_floor` in payload).
2. **Score-primary ordering** (dependsOn ranks only break near-ties).
3. **Default `max_steps` = 3** (was 6).
4. Automation: `UeremcpCore.ReferenceToolset.ResolveIntent.PlanScoreGate`.

Still open: separate `plan_confidence` field; richer domain filters (`1b.3`).

**1.4 Unify tool naming.** P — UEREMCP uses `CreateNiagaraEffect`, Epic's Python
toolsets use `create_material`. Two conventions in one surface guarantees wrong
guesses; I made this exact error. Breaking change across six workstreams, so it
needs a decision, not a patch.

## Tier 1b — agent-reported gaps, 2026-07-30 (validated)

From a working visual agent using the shipped router end to end. Assessed and
accepted except where noted.

### The call-count arithmetic — read this first

The design target is **2 calls**: ask what to use and get the full contract, then
do it. Observed is **5**. This table is the whole backlog in miniature:

| # | Call | Exists because | Removed by |
|---|---|---|---|
| 1 | `ResolveIntent` | find the tool | keep |
| 2 | `DescribeOperation` | call 1 returns `{requestJson: string}`, not the real schema | **1b.1** |
| 3 | the operation | the work | keep |
| 4 | `GetJobResult` | cold Niagara compile returns `partially_completed` | **1b.5** |
| 5 | capture | separate call | **1b.6** |

**1b.1 alone takes 5 → 3. Adding 1b.5 and 1b.6 reaches the 2-call target.**
Prioritise strictly in that order; the others are quality, not count.

**1b.1 Publish full nested schemas. HIGHEST VALUE — do this first, alone.**
`DescribeOperation` and tool descriptors must return the inner `specification`
schema — enums, required fields, defaults — plus a filled `request_json` for the
happy path and one non-trivial path. No new tools required: the schemas already
exist under `schemas/domains/**` and `FToolset::GetJsonSchema()` is overridable
`[VERIFIED: $TR/Public/ToolsetRegistry/Toolset.h]`. This is a publishing change,
not a redesign. See 1.2a.

**1b.2 Template authoring — ACCEPTED. Seeded built-ins — REJECTED.**
Accept: `CreateTemplate` / `AuthorTemplateFromScratch` (emitter roles, materials,
meshes, timing, user params); `PromoteToTemplate` writing a real template asset
and id; `SearchTemplates` returning id, description, required inputs, preview
path; `InstantiateTemplate` with inputs, modifiers and honest `dry_run`.

Reject: shipping `niagara.cast.helix_ring.v1` and friends. **The MCP must not ship
a library** — per the operator, the agent builds its own per game. Seeded content
rots, and it biases every agent toward whatever was seeded. An empty library is
the correct starting state; an agent that cannot fill it is the bug. See 1.3b.

**1b.3 Router ranking — see 1.3d**, plus one addition worth having: when
`existing_assets` are supplied, prefer inspect/capture/modify over
create-from-scratch. Also accept explicit `domain` filters
(`niagara|material|capture`).

**1b.4 Richer `CreateNiagaraEffect` contract.** Extends 1.3b-iii. Specification
should carry `effect_type` (projectile|cast|aura|slash|impact|beam), components
with role semantics (`ground_circle`, `helix_mesh`, `annular_smoke`, `sparks`,
`wave`), `asset_refs` for materials/meshes/textures by path, timing
(duration, one-shot vs loop), and scale/radius. **Response must report which roles
were satisfied versus stubbed** — that is the honesty requirement, not a nicety.

**1b.5 Capture must be terminal in one call.** Block until the renderer is warm or
retry internally; return a terminal result rather than `partially_completed` plus
a poll. Two real bugs alongside it:
- **Output landed in the RE project, not the open one.** Hardcoded path; must
  write under the *current* project's `Saved/UEREMCP/...`.
- **"Trailer bytes break image viewers."** Likely the same defect recorded in
  `VISUAL_CAPTURE_PROTOCOL.md` 4.1/4.3: a float render target exported under a
  `.png` name produces **EXR bytes**. Verify the target is `RTF_RGBA8` before
  looking anywhere else.
Also wanted: optional `make_gif` / contact sheet — see VISUAL_CAPTURE_PROTOCOL §10.

**1b.6 One goal-level envelope.** A single MCP tool that resolves, creates or
instantiates, validates, captures, and returns frame paths. Internally multi-step
is fine; expose a `job_id` **only when genuinely long-running**, not by default.
This is what removes call 5.

**1b.7 Response hygiene.**
- **Stop the proxy's "BATCH THIS WORK" banners on legitimate UEREMCP domain
  calls.** They cost tokens on every response and push agents away from calls that
  are already correctly batched.
- `detail: compact|full` — the create reply was reported as huge; topology behind
  `full`.
- Stable status enums, and a `next_tool` field on every response.
- Registry hash plus a "tools changed" flag, so agents stop re-discovering each
  turn. *This and `next_tool` attack round-trip count directly and rank higher
  than their position here suggests.*

### Acceptance tests must assert round-trip count

Not just correctness. "It works in 5 calls" is precisely the failure this section
exists to remove, and a test that checks only the output would pass it. Every item
above needs a test asserting the number of MCP round trips, per
`BENCHMARK_PROTOCOL.md`.

## Tier 2 — discoverability machinery (built this session)

| # | Item | State |
|---|---|---|
| 2.1 | `tools/dump_tool_registry.py` — generated ground truth | **V**, working |
| 2.2 | `tools/check_tool_names.py` — CI check, self-tested | **V**, 0 problems, catches bogus names |
| 2.3 | `tools/gen_focus_config.py` + plugin ini — hide superseded primitives | **V**, validated, **not enabled** |
| 2.4 | `tools/route_prototype.py` + [`TOOL_ROUTER.md`](TOOL_ROUTER.md) | **V** prototype, blocked on 1.1 |
| 2.5 | Extend `check_tool_names` to validate **domains**, not just tool names | **P** — would have caught 0.3 |

**Do not enable focus mode (2.3) before 1.1 and 1.2 land.** Blocking the
primitives while UEREMCP's own tools remain undiscoverable leaves an agent with
no path at all. It also only hides 140 of 911 tools — it addresses the specific
fallback, not scale. Only routing does that.

## Tier 3 — visual verification

**3.1 Compile `UeremcpVisualCaptureToolset`.** W — `capture_effect_frames`,
deterministic per-frame Niagara capture. Transcribed from the operator-proven
`Scripts/capture_ice_wall_baseline0.py`; the recipe is proven, the C++ is not.

**3.2 Extend capture beyond Niagara.** P — it is Niagara-only today. Worlds,
materials, and animation all need the same rig.

**3.3 Isolate the `GetSystemSummary` crash.** P — one occurrence, log ends on that
dispatch, an older dump in the same project shows an access violation topped by
`UnrealEditor_NiagaraEditor`. Correlation, not confirmed repro. Reproduce in a
throwaway project before any automated path calls it.

## Tier 4 — coverage gaps

Analysis in [`COVERAGE_PLAN.md`](COVERAGE_PLAN.md) Part I. Four registry zeros
(landscape, water, foliage, procedural mesh) plus audio (**1 tool**),
networking (3), world partition (3).

**Implementation spec written** — `COVERAGE_PLAN.md` **Part III**: module layout,
operation set, request schemas, determinism contract, acceptance criteria.

**`UeremcpEnvironment` ALREADY EXISTS and is built.** **V live** — eight
AICallable tools registered; live `list_toolsets` shows
`UeremcpEnvironment.UeremcpEnvironmentToolset` (`0.3.0-environment-acceptance`).
Dependencies and plan registration are correct.

**Deploy path was the blocker; it is fixed.** RE junction target is
`$UEREMCP_DEPLOY\Plugins\UEREMCP`
(not `UEREMCP-ws01`). Historical note below kept for forensics only.

**Router integration needs no work.** Verified by simulation: injecting the eight
tools into the index and running a cold plain-text intent
("mountains with a valley and a river running through it, trees along the banks,
rain that follows the camera") returns `BuildEnvironment` (80.5), `CreateWaterBody`
(77.0), `ScatterFoliage` (72.4), `AttachWeather` (62.1) — correct tools, correct
order, no hand-registration. The index is generated from the live registry, so
they appear the moment the module is loaded.

**Its descriptions are the model for item 1.1.** They are written in task
vocabulary with inline worked examples — *"Use when: forest along river banks
with a clear channel"* — which is exactly why the router finds them instantly,
while the existing 21 tools written in architecture vocabulary score 2/7. Copy
this style when rewriting them.

**Important correction from the live API probe.** `Water`, `Foliage`, `Spline`
and `PCG` classes are all reachable via Python **today** — the registry shows zero
tools because nothing *wraps* them. That is binding work, not capability work,
and much cheaper than Part I implied. Landscape sculpting genuinely is not
reachable (`AlphaBrush`, `LandscapeEditorObject` absent) — heightmap import is the
path. GeometryScript is blocked on 0.1.

Two operations the reference request exposed, missing from the first draft:
**`attach_weather`** (camera-following rain is its own operation, and must be
PIE-proven by moving the pawn ≥10m, not by placing an emitter near spawn), and a
**non-Niagara capture path** (`capture_effect_frames` takes a `UNiagaraSystem`; a
world screenshot needs `capture_viewport_to_disk` or a widened action).

Suggested order: audit PCG (may already cover scatter) → GeometryScript →
landscape via heightmap import → water → audio → world partition.

---

## 5. Procedural generation — what would actually help

You asked specifically. Ranked by leverage.

**5.1 GeometryScript is the single highest-value unlock.** Booleans, extrusion,
sweeps along splines, mesh-to-static-mesh baking. One capability yields buildings,
modular kits, props, greebling, *and* terrain meshes. It has no registered toolset
and is disabled in the project — so it is currently invisible twice over.

**5.2 Heightmap-first terrain, not sculpt strokes.** Generating a heightmap array
and importing it sidesteps the landscape editor API almost entirely — which is
the editor-only, undocumented, version-fragile part. It is also deterministic and
diffable by construction. Far cheaper than scripting sculpt operations.

**5.3 Spline-driven everything.** Rivers, roads, walls, fences, cliff edges and
riverbank exclusion corridors are all the same primitive: a spline plus a
generation rule. One spline abstraction covers most of "build a world", and it
composes with GeometryScript sweeps and PCG scatter.

**5.4 Seeds are mandatory, not optional.** Every procedural action must take a
seed and be a pure function of (seed, parameters). Without it there is no
regression test, no diff, and no way to answer "did my change do that, or was it
just different this time?" This is the same property that makes the capture
harness a gate rather than an impression — and it is much cheaper to design in
now than to retrofit.

**5.5 Follow the precedent that already exists.** `UeremcpMaterial` already ships
`CreateProceduralTexture`. Procedural asset generation is not a new pattern here —
copy its envelope shape, parameter handling, and validation approach rather than
inventing a second one.

**5.6 Noise as a shared service, not per-domain.** Terrain height, foliage
density, building variation, and texture detail all want the same
seeded/tileable/layerable noise. Build it once as a plan-level primitive; every
procedural domain consumes it.

**5.7 `ExecutePlan` is already the batching vehicle.** `UeremcpCore` exposes
`ExecutePlan`/`GetJobResult`/`CancelJob`, and `plan.schema.json` already models
dependencies with `$ref` chaining. A world build is a plan —
terrain → river → foliage → structures → capture — and world-building is where
this pays off hardest, because one call producing 8,000 instances against
`N·C₀ + r·N²/2` is the best operations-per-call ratio anywhere in the engine.
**Do not build a second batching layer.**

**5.8 Verification is genuinely unsolved for worlds.** A pixel-delta gate proves
an effect rendered; nothing proves a landscape is *good*. Expect world-building to
lean on human review far more than other domains, and do not let a screenshot
masquerade as a gate. Determinism (5.4) is the only real defence: you can at least
prove the world did not change unintentionally.

---

## The uncomfortable one

`CAPABILITY_CATALOG.md` still reads *"Nothing is `available` yet — Phase 0"*, and
32 branches are unmerged. **Finishing the seven existing domains is worth more
than starting four new ones.** Tier 4 is the exciting work and Tier 0–1 is what
makes any of it usable; taken in the wrong order, the surface grows faster than
agents' ability to find anything in it — which is the problem this session
measured, not a hypothetical.
