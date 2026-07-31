# UEREMCP Tool Composability Audit — 2026-07-30

**Owner:** WS-01 (proposal)  
**Evidence base:** deploy tip `7745d3b` (`ws-01-router-registration-integration`), static code/schema audit  
**Registry note:** Root `tools/registry_snapshot.json` (2026-07-30 live dump) lists **21** UEREMCP tools and omits Environment, Systems, VisualCapture, intent-router tools, and `CreateSpellVariation`. This audit uses **42** callables from deploy-tip headers + module registration. Live `list_toolsets` was not re-called in this session; re-verify after merge.

**Context read (not edited):** dirty-root `docs/BACKLOG.md`, `docs/COVERAGE_PLAN.md`.

---

## In-flight parallel work (do not duplicate here)

| Commit / branch | Owner | Scope |
|---|---|---|
| `418374c` / `ws-16-environment-coverage` | WS-16 | Opt-in `include.*` defaults for `BuildEnvironment` (replaces all-stages-on when `include` omitted) |
| `60422bb8` (pending merge) | WS-16 | Flexible environment schema v2 — **not started on this follow-on**; see cross-ref only |
| `ea3fcc74` (pending merge) | WS-01 | Merge opt-in includes into deploy integration tip |
| `71099637` / `ws-16-rain-niagara-create` | WS-16 | Real Niagara rain creation in `BuildEnvironment` / weather stage |
| `db16068c` (pending merge) | WS-01 | Tier 1b nested `GetJsonSchema()` per toolset (P0 schema bottleneck) |

This follow-on closes **P0-A/B/C** from the table below and records the audit; it does **not** implement BuildEnvironment v2 or Niagara role generalization.

---

## Executive summary — top systemic issues

1. **`{requestJson: string}` discovery bottleneck (all 42 tools).** No toolset overrides `GetJsonSchema()`; MCP exposes a single opaque string while domain schemas exist under `schemas/domains/**`. Agents cannot compose without `ResolveIntent`/`DescribeOperation` or repo access. [VERIFIED: no `GetJsonSchema` in `Plugins/UEREMCP/**`; BACKLOG 1.2a]

2. **`BuildEnvironment` is a scripted mountain→river→forest→rain→overcast-lighting pipeline** with stage defaults that enable everything except structures/capture. Sub-tools (`CreateLandscape`, etc.) exist for `ExecutePlan`, but the monolith remains the documented “prefer” path and router catalog anchor. [VERIFIED: `UeremcpEnvironmentToolset.h:14-16`, `UeremcpEnvironmentService.h:30-37`, `UeremcpEnvironmentService.cpp:575-880]

3. **`fallback_policy` is schema-documented but not enforced.** Parsed into `FUeremcpEnvironmentBuildSpec::FallbackPolicy` and never read during build; rain/cube-foliage/river approximations always proceed with warnings. [VERIFIED: `UeremcpEnvironmentService.cpp:264` parse only; grep shows no other `FallbackPolicy` use] — **Fixed** on `ws-01-composability-p0-followon`: `prefer_real` rejects streak-rain/cube fallbacks; `allow_approximate` sets top-level `approximated:true`.

4. **`CreateNiagaraEffect` is role-locked to a fireball-shaped POC B vocabulary** — six hardcoded Epic emitter templates, material purposes mapped to `elemental_projectile_core|trail`, probe paths only under `__UeremcpTests`/`__UeremcpPoc`. Empty `components` creates a scaffold with **zero** role emitters; validation gates still assume six default roles elsewhere. [VERIFIED: `UeremcpNiagaraRoleNames.cpp:26-52`, `UeremcpNiagaraCreate.cpp:675-704`, `UeremcpNiagaraPocBGates.cpp:9-19]

5. **Template library cannot close the composability loop.** `SearchTemplates` returns empty on day one (by design); `PromoteToTemplate` is preview-only; no `CreateTemplate`. Agents cannot bank custom patterns — every job re-enters Niagara/Material fireball slice. [VERIFIED: `UeremcpTemplateService.cpp:571`, BACKLOG 1.3b]

6. **Intent router couples to a hand-maintained `operation_catalog.json` + hardcoded SUPERSEDED map** instead of delegating to schemas. Adding Epic capabilities does not automatically improve routing unless catalog entries are added. [VERIFIED: `UeremcpIntentRouter.cpp:326-335`, `Content/IntentRouter/operation_catalog.json`]

7. **Scratch-root path gating is consistent but couples all domains to RE probe layout** (`/Game/__UeremcpTests/`, `/Game/__UeremcpPoc/`). Composable for safety; not composable for production content paths without policy expansion.

---

## Method

| Source | Use |
|---|---|
| `UFUNCTION` doc comments | MCP `describe_toolset` surface |
| `ParseBuildSpec`, `ParseCreate*`, `FUeremcpIntentRouter` | Hidden defaults & fallbacks |
| `schemas/domains/**` | Contract vs implementation |
| `operation_catalog.json` | Router dependencies & examples |
| Tests (`*Tests.cpp`, `*.spec.cpp`) | Acceptance gates & honest statuses |

Flags per tool:

- **H** — hardcoded assumptions (paths, presets, Epic tool names, domain coupling)
- **D** — hidden defaults when caller omits fields
- **F** — silent or soft fallbacks (approximation, partial success as success)
- **S** — scripted multi-step workflow vs composable single stage

---

## Master table — all 42 UEREMCP AICallable tools

| Toolset | Tool | H | D | F | S | Composability verdict |
|---|---|:---:|:---:|:---:|:---:|---|
| **UeremcpCore** | GetStarted | | ✓ | | | Bootstrap only; composable |
| | ResolveIntent | ✓ | ✓ | ✓ | ✓ | Scripted plan from catalog + lexical rank; not a generic orchestrator |
| | DescribeOperation | ✓ | | | | Composable; depends on catalog examples |
| | Ping | | | | | Composable probe |
| | Echo | | | | | Composable probe |
| | ExecutePlan | | ✓ | | | **Composable orchestrator** — correct aggregation point |
| | GetJobResult | | | | | Composable poll |
| | CancelJob | | | | | Composable cancel |
| **UeremcpEnvironment** | BuildEnvironment | ✓ | ✓ | ✓ | ✓ | **Monolithic scripted pipeline**; split mentally into stage tools |
| | CreateLandscape | ✓ | ✓ | | | Composable stage; forces terrain-only |
| | CreateWaterBody | ✓ | ✓ | ✓ | | Composable stage; Water plugin / skip fallback |
| | ScatterFoliage | ✓ | ✓ | ✓ | | Composable stage; cube mesh fallback |
| | AttachWeather | ✓ | ✓ | ✓ | ✓ | Stage bundles rain **+** lighting |
| | PlaceStructures | ✓ | ✓ | | | Composable stage; GeometryScript boxes along river |
| | InspectEnvironment | ✓ | | | | Composable read |
| | ValidateEnvironment | ✓ | ✓ | | | Composable gates; PIE weather follow optional |
| **UeremcpSystems** | CreateAudioCue | ✓ | ✓ | | | Composable; SoundCue-only (no MetaSound) |
| | InspectAudio | | ✓ | | | Composable read |
| | ValidateReplication | ✓ | ✓ | | | Composable audit; Pattern B examples |
| | InspectWorldPartition | | ✓ | | | Composable read |
| | RepairWorldPartition | ✓ | ✓ | | | Composable; mutating gated |
| **UeremcpValidation** | CaptureEffectFrames | ✓ | ✓ | | | Composable; requires `validate=true` |
| | CaptureWorldFrames | | ✓ | | | Composable |
| | CaptureMaterialFrames | | ✓ | | | Composable |
| | CaptureAnimationFrames | ✓ | ✓ | | | Composable; mesh path fallback rules |
| **UeremcpNiagara** | Echo | | | | | Probe |
| | InspectSystem | ✓ | | ✓ | | Composable read; lossy stacks |
| | CreateNiagaraEffect | ✓ | ✓ | ✓ | ✓ | **Scripted POC B slice**; roles/templates fixed |
| **UeremcpMaterial** | Echo | | | | | Probe |
| | CreateVfxMaterial | ✓ | ✓ | ✓ | | Composable envelope; **purpose whitelist** |
| | CreateProceduralTexture | ✓ | ✓ | | | Composable; scratch paths |
| **UeremcpBlueprint** | Ping | | | | | Probe |
| | Echo | | | | | Probe |
| | ReadGraph | | ✓ | | | Composable read (ADR-0004) |
| | SubmitGraph | | ✓ | | | Composable write (ADR-0004) |
| **UeremcpGameplay** | CreateSpell | ✓ | ✓ | | | Composable spec; RE table + scratch path |
| | CreateSpellVariation | ✓ | | | | Composable clone/variation |
| **UeremcpAnimation** | InspectMontage | | | ✓ | | Composable read; **partial response** |
| | ReadAnimBp | | | ✓ | | Composable read; authoring unsupported |
| **UeremcpTemplates** | SearchTemplates | | | | | Composable; empty store |
| | InstantiateTemplate | ✓ | ✓ | | | Composable; needs `template_id` |
| | PromoteToTemplate | | | ✓ | | **Non-composable bank** (preview-only) |

---

## Worst offenders (file:line)

| Issue | Location |
|---|---|
| All environment stages default on in `BuildEnvironment` | [VERIFIED: `UeremcpEnvironmentService.h:30-37`] |
| `include.*` only applied when `include` object present; omitted → full scene | [VERIFIED: `UeremcpEnvironmentService.cpp:267-280`] |
| Rainy overcast lighting preset hardcoded | [VERIFIED: `UeremcpEnvironmentService.cpp:828-878`] |
| Instanced streak rain when `rain_system_path` empty | [VERIFIED: `UeremcpEnvironmentService.cpp:903-912`] |
| Engine cube foliage placeholder | [VERIFIED: `UeremcpEnvironmentService.cpp:698-706`] |
| `fallback_policy` parsed, never enforced | [VERIFIED: `UeremcpEnvironmentService.cpp:264`] |
| `AttachWeather` stage forces lighting | [VERIFIED: `UeremcpEnvironmentToolset.cpp:96-99`] |
| Niagara role → Epic template map | [VERIFIED: `UeremcpNiagaraRoleNames.cpp:26-40`] |
| Default POC B six-component gate | [VERIFIED: `UeremcpNiagaraRoleNames.cpp:43-52`] |
| Projectile effect swaps Minimal→Looping template | [VERIFIED: `UeremcpNiagaraCreate.cpp:611-621`] |
| Material purpose whitelist | [VERIFIED: `UeremcpMaterialService.cpp:809-818`] |
| Router SUPERSEDED hardcode | [VERIFIED: `UeremcpIntentRouter.cpp:327-335`] |
| Plan score-gate 35% of best | [VERIFIED: `UeremcpIntentRouter.cpp:737-739`] |
| Capture defaults: 8 frames, 1.5s, 960×540, `three_quarter` | [VERIFIED: `UeremcpVisualCaptureToolset.cpp:147-151`] |
| Promotion preview-only | [VERIFIED: `UeremcpTemplateService.cpp:571`] |

---

## Bottleneck ranking (if Epic shipped 500 more capabilities tomorrow)

| Rank | Bottleneck | Why it breaks scale |
|:---:|---|---|
| 1 | **`{requestJson: string}` on every UEREMCP tool** | Agents cannot discover parameters; drift to Epic typed tools. No per-action schema at MCP boundary. |
| 2 | **`ResolveIntent` + `operation_catalog.json`** | New tools appear in registry but routing quality needs manual catalog rows, aliases, `depends_on_actions`, examples. Lexical BM25 over descriptions does not generalize. |
| 3 | **`BuildEnvironment` monolith** | Encodes full biome scene; cannot reuse stages without knowing sub-tools exist. Router catalog says “prefer composite”. |
| 4 | **`CreateNiagaraEffect` role/template map** | Each new VFX archetype needs C++ role entries + Epic template paths; not data-driven. Unknown roles fall back to `sparks` template. [VERIFIED: `UeremcpNiagaraRoleNames.cpp:40`] |
| 5 | **`CreateVfxMaterial` purpose whitelist** | New material families need C++ gate; cannot delegate to MaterialTools batch internally without expanding whitelist. |
| 6 | **Template bank (`PromoteToTemplate` preview)** | No path from “proved asset” → reusable spec; agents repeat full create pipelines. |
| 7 | **Hardcoded SUPERSEDED demotion map** | New Epic toolsets won’t map to UEREMCP semantic replacements without code edit. |
| 8 | **`ExecutePlan` action registry** | Composable, but each new domain must register `RegisterAction` handler; no auto-bridge from registry. |
| 9 | **Scratch path policy** | Correct for safety; blocks composing into arbitrary `/Game/` until policy tiers land (WS-12). |
| 10 | **Thin Epic wrappers without semantic batching** | REAgentTools/Epic primitives remain 800+ typed tools — router demotes but does not hide; agent overload persists. |

---

## Refactor priority (P0–P3)

| Pri | Item | Effort | Outcome | Follow-on status |
|:---:|---|:---:|---|---|
| **P0** | Override `GetJsonSchema()` per toolset/action using `schemas/domains/**` | M (2–3 wk) | Removes #1 bottleneck; enables real composition at MCP | **In flight** — `db16068c` |
| **P0** | Enforce `fallback_policy` in Environment (fail or downgrade status when `prefer_real`) | S (2–3 d) | Stops silent cube-rain/streak success | **Closed** — `ws-01-composability-p0-followon` |
| **P0** | Un-preview `PromoteToTemplate` + `CreateTemplate` | M (1–2 wk) | Closes reuse loop (BACKLOG 1.3b) | open — WS-15 |
| **P0** | Registry snapshot freshness (`check_tool_names` / dump gate) | S (1 d) | Snapshot cannot lag source callables | **Closed** — count + fingerprint gate |
| **P0** | Router/catalog coupling CI (`check_operation_catalog.py`) | S (1 d) | Catalog `qualified` ⊆ registry | **Closed** — validation script + `GetLogEntries` fix |
| **P1** | Split `BuildEnvironment` doc/router guidance to **stage-first**; demote monolith to `execute_plan` preset | S (3–5 d) | Composable world builds via plan | partial — `418374c` opt-in includes |
| **P1** | Data-drive Niagara roles (`schemas/domains/niagara/roles.v1.json`) | M (2 wk) | New effects without C++ map edits |
| **P1** | Expand Material `purpose` via schema registry, not `if` chain | M (1–2 wk) | Material composability |
| **P1** | Router: generate catalog from schemas + `RegisterAction` table | L (3–4 wk) | Scales with new tools |
| **P2** | `AttachWeather`: decouple lighting from rain stage | S (1–2 d) | True weather-only composition |
| **P2** | `CreateNiagaraEffect`: explicit `components` default policy (empty = error vs explicit minimal set) | S (2–3 d) | Removes ambiguous empty create |
| **P2** | `InspectMontage`: ship full asset-state response | S (3–5 d) | Removes partial fallback |
| **P3** | MetaSound goal API or honest permanent gap in Systems | L | Audio composability |
| **P3** | Production path tiers beyond `__UeremcpTests` | M | WS-12 policy integration |

Effort: **S** = days, **M** = 1–2 weeks, **L** = multi-week.

---

## Recommended architecture direction

**Use a generic orchestrator pattern for multi-stage outcomes; keep domain tools as single-responsibility semantic ops.**

```
Agent goal
    → ResolveIntent (discovery only, not execution)
    → ExecutePlan { operations: [ single-action envelopes with $ref ] }
         OR single domain tool when one semantic op suffices
    → GetJobResult / Capture* for verification
```

| Layer | Responsibility | Current state |
|---|---|---|
| **Orchestrator** | `ExecutePlan`, job poll/cancel | Good substrate (ADR-0008/0009) |
| **Router** | Intent → ranked tools + examples | Useful; should not encode fixed pipelines |
| **Domain semantic ops** | One validated outcome per call | Strong for Blueprint, weak for Environment monolith & Niagara roles |
| **Primitives** | Epic toolsets | Internal only; router demotes, does not hide |

**Do not** add more monolithic `Build*` tools. **Do** add plan-registerable stage actions (Environment sub-tools are the right pattern), data-driven role/template specs, and MCP-visible schemas.

**Composable vs scripted rule of thumb:** If omitting a field runs more than one engine mutation the caller did not name, flag as scripted and offer either `include.*` toggles or plan decomposition.

---

## Appendix A — Per-tool mandatory blocks

### UeremcpCore.UeremcpReferenceToolset

#### GetStarted
- **Single responsibility:** Return bootstrap briefing pointing agents to `ResolveIntent` and envelope rules.
- **Default assumptions:** None required; empty `specification` OK.
- **Composable vs scripted:** Composable. No change needed.

#### ResolveIntent
- **Single responsibility:** Map plain-text intent to ranked live-registry tools and an ordered plan with example envelopes.
- **Default assumptions:** `mode=recommend`; `MaxSteps` capped; UEREMCP tools scored 1.6×; probe tools excluded unless query mentions ping/echo; plan includes one tool per toolset (best per toolset); score floor 35% of best hit.
- **Composable vs scripted:** **Scripted plan assembly** from `operation_catalog.json` dependencies + per-toolset winner. **Make composable:** Plan = top-N scored hits only (not best-per-toolset), optional `plan_style=minimal|full`; never auto-include domains below score floor; do not treat `execute_if_complete` as available until implemented.

#### DescribeOperation
- **Single responsibility:** Return live-registry description, `{requestJson}` example from catalog, and safety notes for one tool.
- **Default assumptions:** Tool name normalized (snake/Pascal); rejects unknown names.
- **Composable vs scripted:** Composable. Improve by attaching full JSON Schema from `schemas/domains/**`.

#### Ping / Echo
- **Single responsibility:** Liveness and envelope round-trip probes.
- **Default assumptions:** None / empty specification.
- **Composable vs scripted:** Composable.

#### ExecutePlan
- **Single responsibility:** Run a declared array of domain actions with `$ref` chaining, rollback, and consolidated manifest.
- **Default assumptions:** `operations` required; unregistered actions fail closed; `dry_run` from envelope options.
- **Composable vs scripted:** **Composable orchestrator** — primary pattern for multi-stage work. Expand by registering every stage action (Environment already does).

#### GetJobResult / CancelJob
- **Single responsibility:** Poll or cancel ADR-0009 jobs.
- **Default assumptions:** `job_id` required.
- **Composable vs scripted:** Composable.

---

### UeremcpEnvironment.UeremcpEnvironmentToolset

#### BuildEnvironment
- **Single responsibility:** Build, save, and structurally validate a full seeded scratch-map environment (terrain, river, forest, rain, overcast lighting).
- **Default assumptions:** `seed` required; target under `__UeremcpPoc` or `__UeremcpTests`; all `bInclude*` true except structures/capture; river width 600, forest bank 3500, max foliage 800, slope limit 55°; weather follow `player_camera`; `fallback_policy=prefer_real` (unused); map lifecycle owned (load/save).
- **Composable vs scripted:** **Scripted.** Runs fixed multi-stage pipeline with rainy preset. **Refactor:** Document as “preset execute_plan”; default `include` to false except named stages; enforce `fallback_policy`; remove bundled lighting from rain unless requested.

#### CreateLandscape
- **Single responsibility:** Import heightmap terrain only (`ALandscape::Import`).
- **Default assumptions:** Same terrain defaults as build spec; only terrain stage enabled via `ApplyStageIncludes`.
- **Composable vs scripted:** Composable stage for plans. Caller `include.*` ignored when using stage tool.

#### CreateWaterBody
- **Single responsibility:** Spawn `AWaterBodyRiver` along seeded spline with `bAffectsLandscape=false`.
- **Default assumptions:** River-only stage; valley already in heightmap if landscape not run in same map.
- **Composable vs scripted:** Composable. **Fallback:** spawn null → warning, `failed_validation` if river required.

#### ScatterFoliage
- **Single responsibility:** Seed HISM instances on river banks with exclusion corridor.
- **Default assumptions:** Forest-only stage; cube mesh if `biome.mesh_path` missing.
- **Composable vs scripted:** Composable. **Refactor:** Fail when `prefer_real` and mesh missing.

#### AttachWeather
- **Single responsibility:** Spawn camera-following rain actor (Niagara or streak fallback).
- **Default assumptions:** Enables rain **and** lighting stages together.
- **Composable vs scripted:** Partially scripted (bundled lighting). **Refactor:** Split `AttachRain` vs `ApplyLighting`.

#### PlaceStructures
- **Single responsibility:** Place GeometryScript boxes along river spline.
- **Default assumptions:** Structures-only; count default 6; requires GeometryScript.
- **Composable vs scripted:** Composable.

#### InspectEnvironment / ValidateEnvironment
- **Single responsibility:** Read metrics / run structural acceptance gates (not pixel gates).
- **Default assumptions:** Validate optionally requires 10 m weather follow in PIE.
- **Composable vs scripted:** Composable verification.

---

### UeremcpSystems.UeremcpSystemsToolset

#### CreateAudioCue
- **Single responsibility:** Create/update `USoundCue` (+ optional attenuation) from wave paths.
- **Default assumptions:** Scratch paths only; dry-run preferred.
- **Composable vs scripted:** Composable single op. **Gap:** no MetaSound graph authoring.

#### InspectAudio
- **Single responsibility:** Read cue/wave/attenuation wiring.
- **Default assumptions:** `include_wave_paths` optional.
- **Composable vs scripted:** Composable.

#### ValidateReplication
- **Single responsibility:** Audit (and optionally fix) Blueprint replication flags in one call.
- **Default assumptions:** Pattern B / server authority examples in docs; dry-run default for mutating fixes.
- **Composable vs scripted:** Composable semantic batch over BlueprintTools primitives.

#### InspectWorldPartition / RepairWorldPartition
- **Single responsibility:** Read WP state / enable partition+streaming on editor world.
- **Default assumptions:** Repair defaults `dry_run=true`; mutate needs `allow_destructive`.
- **Composable vs scripted:** Composable.

---

### UeremcpValidation.UeremcpVisualCaptureToolset

#### CaptureEffectFrames
- **Single responsibility:** Deterministic Niagara render + pixel delta vs empty stage.
- **Default assumptions:** `options.validate=true` required; 8 frames, 1.5s, 960×540, `three_quarter` camera if omitted.
- **Composable vs scripted:** Composable verification. Cold compile may return `partially_completed` → poll job.

#### CaptureWorldFrames
- **Single responsibility:** Editor world SceneCapture with warm-up ticks.
- **Default assumptions:** Optional target label; frame/warm-up/camera defaults in implementation.
- **Composable vs scripted:** Composable; preferred over `BuildEnvironment` screenshot hook.

#### CaptureMaterialFrames / CaptureAnimationFrames
- **Single responsibility:** Disposable stage capture for materials / posed animation.
- **Default assumptions:** Animation requires `skeletal_mesh_path` unless on sequence.
- **Composable vs scripted:** Composable.

---

### UeremcpNiagara.UeremcpNiagaraToolset

#### Echo
- **Single responsibility:** Envelope probe.
- **Default assumptions:** None.
- **Composable vs scripted:** Composable.

#### InspectSystem
- **Single responsibility:** Export Niagara topology to ADR-0004 graph + `extensions.niagara`.
- **Default assumptions:** Probe paths; event handler stacks lossy.
- **Composable vs scripted:** Composable read.

#### CreateNiagaraEffect
- **Single responsibility:** Create/replace probe Niagara system from effect_type, element, and component roles with compile/save/validate gates.
- **Default assumptions:** `effect_type` required; `components` omitted → **no emitters added**; projectile uses looping system template; roles map to fixed Epic emitter templates; inline materials use fireball element presets; paths under scratch roots only.
- **Composable vs scripted:** **Scripted POC B workflow.** **Refactor:** Schema-driven roles; `components` required or explicit `preset=poc_b_fireball`; freeform module graph via `submit_graph` parity or template instantiation.

---

### UeremcpMaterial.UeremcpMaterialToolset

#### Echo
- **Single responsibility:** Envelope probe.
- **Composable vs scripted:** Composable.

#### CreateVfxMaterial
- **Single responsibility:** Create/update elemental projectile MI from purpose + element + features.
- **Default assumptions:** `purpose` required; only `elemental_projectile_core|trail` (+ fireball aliases); element defaults from JSON or C++ fallback; features default from purpose.
- **Composable vs scripted:** Composable envelope, **non-composable capability surface**. Refactor: purpose registry in schema.

#### CreateProceduralTexture
- **Single responsibility:** Generate noise/mask `Texture2D` under scratch paths.
- **Default assumptions:** `generate` required; dimensions/seed defaults in schema.
- **Composable vs scripted:** Composable.

---

### UeremcpBlueprint.UeremcpBlueprintToolset

#### Ping / Echo
- **Single responsibility:** Probes.
- **Composable vs scripted:** Composable.

#### ReadGraph
- **Single responsibility:** Full Blueprint graph JSON + revision (ADR-0004).
- **Default assumptions:** `target.asset_path`; `graph_id` optional (EventGraph default in bridge).
- **Composable vs scripted:** Composable — model for other domains.

#### SubmitGraph
- **Single responsibility:** Replace graph from complete JSON with compile/save/re-read validation.
- **Default assumptions:** `expected_revision`, `mode=replace`, dry-run supported.
- **Composable vs scripted:** Composable — gold standard semantic op.

---

### UeremcpGameplay.UeremcpGameplayToolset

#### CreateSpell
- **Single responsibility:** Upsert one `FREAbilityDef` row with delivery, costs, networking Pattern B checks.
- **Default assumptions:** Scratch DataTable path; dry-run → `partially_completed` plan only; many required spec fields per schema.
- **Composable vs scripted:** Composable single row authoring; RE-specific coupling.

#### CreateSpellVariation
- **Single responsibility:** Clone spell row changing presentation soft paths only; verify protected fields.
- **Default assumptions:** `source_binding`, `verification_mode` required.
- **Composable vs scripted:** Composable variation pattern (good exemplar).

---

### UeremcpAnimation.UeremcpAnimationToolset

#### InspectMontage
- **Single responsibility:** Inspect montage slots, sections, notifies, root motion.
- **Default assumptions:** Returns honest partial until asset-state envelope accepted.
- **Composable vs scripted:** Composable read with **silent partial** — fix by complete response.

#### ReadAnimBp
- **Single responsibility:** Read AnimBP graphs via shared ADR-0004 reader.
- **Default assumptions:** Read-only; state machine authoring unsupported (capability note).
- **Composable vs scripted:** Composable read.

---

### UeremcpTemplates.UeremcpTemplatesToolset

#### SearchTemplates
- **Single responsibility:** Query in-memory template store.
- **Default assumptions:** Empty store on fresh install; optional query/domain filters.
- **Composable vs scripted:** Composable; useless until promotion works.

#### InstantiateTemplate
- **Single responsibility:** Materialize assets from `template_id` + inputs.
- **Default assumptions:** `template_id` required; unknown id fails.
- **Composable vs scripted:** Composable when library populated.

#### PromoteToTemplate
- **Single responsibility:** Preview promoting scratch asset to template (no write).
- **Default assumptions:** Preview/quarantine; dry-run defaulted.
- **Composable vs scripted:** **Not composable for reuse** until write path enabled.

---

## Appendix B — Registry vs deploy tip drift

| Deploy tip (7745d3b) | Root `registry_snapshot.json` |
|---|---|
| 10 UEREMCP toolsets, 42 tools | 7 toolsets, 21 tools |
| Environment (8), Systems (5), VisualCapture (4) | Missing |
| GetStarted, ResolveIntent, DescribeOperation | Missing |
| CreateSpellVariation | Missing |

Re-dump after merge: `python tools/dump_tool_registry.py` (or live MCP `list_toolsets`).

---

## Appendix C — Tests as composability signals

| Test area | Signal |
|---|---|
| `UeremcpEnvironmentTests` | Default river width 600, forest 3500 when omitted |
| `UeremcpReferenceToolsetTests` ResolveIntent plan score-gate | Weak domains excluded from plan |
| `UeremcpNiagaraRoleNamesTests` | Six-role fireball contract frozen |
| `UeremcpTemplatesToolsetTests` | Promotion never claims `*_validated` |
| `VisualCaptureToolset.spec` | Capture requires validate=true |

---

*End of audit. Action owner: WS-01 for cross-cutting proposals; domain WSs for toolset refactors per `WORK_ALLOCATION.md`.*
