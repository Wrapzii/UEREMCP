# RB-07: Niagara system, emitter, and module-stack read/write

- **Owner:** WS-07
- **Status:** research_complete (implementation gated on Phase 1)
- **Blocks:** POC B, POC C, ADR-0004 confidence for non-Blueprint graphs
- **Priority:** high
- **Branch / worktree:** `ws-07-niagara` @ `$UEREMCP_ROOT-ws07`
- **Last verified:** 2026-07-29

## Framing

Niagara is the domain the owner most wants ("generating effects and templates from the
beginning" — `docs/WHY.md`) and the domain where the shared graph representation is
most likely to fit badly. A Niagara system is not one node graph: it is a system
script, a set of emitters, per-emitter module *stacks* across execution phases
(Emitter Spawn/Update, Particle Spawn/Update, Event handlers), renderers, and a
parameter store — with compiled scripts underneath.

`ADR-0004` anticipates this: `graph_type` includes `NiagaraSystemGraph`,
`NiagaraEmitterGraph`, `NiagaraModuleStack`, and `NiagaraScriptGraph`, and
`extensions.niagara` is yours to define. **If module stacks genuinely cannot be
expressed as `nodes`/`links`, say so with evidence and challenge ADR-0004** — that is
a legitimate and expected outcome, not a failure.

Read `$RAT/Docs/NIAGARA_BATCHING.md` first. It documents a working batching pattern
against Epic's Niagara tools and will save you days.

---

## R-05 verdict (priority)

**Do not fork ADR-0004.** Module stacks map to `graph.schema.json` + `extensions.niagara`
without a separate graph schema.

| Layer | Role |
|---|---|
| `graph_type: NiagaraModuleStack` | One stack (one script usage on one emitter/system). `nodes[]` = modules in execution order; optional sequential `links[]` of `kind: exec` encode order for agents that prefer edge lists. |
| `graph_type: NiagaraEmitterGraph` | Emitter envelope: `subgraphs[]` of stack `graph_id`s + renderer nodes; emitter settings in `extensions.niagara.emitter`. |
| `graph_type: NiagaraSystemGraph` | System envelope: emitters as subgraphs, system scripts, user variables as `variables[]`, dependencies. |
| `graph_type: NiagaraScriptGraph` | True EdGraph of a module/dynamic-input script — **out of POC B/C scope** (composition of existing modules only). |
| `extensions.niagara` | Pressure valve: value modes, dynamic-input chains, renderer/DI JSON blobs, event-handler stacks, inheritance metadata, compile/issue mapping. |

**Why this is not a fork:** ADR-0004 already lists the four Niagara `graph_type` values and
explicitly places family differences in `extensions`. Stacks are *ordered module lists with
typed inputs*, not Blueprint-style free pin graphs. Forcing every input mode into `links`
would be lossy; putting modes + chains in `extensions.niagara` keeps one schema honest.

**Fidelity declaration (honest):** `fidelity.round_trip_supported: false` until
retrieve → `mode: replace` → retrieve hash-stability is proven. Lossy / deferred areas
named below. No ADR challenge filed.

---

## Questions

### A. Reading

#### 1. Enumerating emitters and module stacks

**Primary path (preferred):** Epic `NiagaraToolsets.NiagaraToolset_System` →
`UNiagaraExternalEditUtilities` public editor API
`[VERIFIED: NiagaraExternalSystemEditorUtilities.h; NiagaraToolset_System.h]`.

| Call | Returns |
|---|---|
| `GetSystemSummary` | System name, user variables, per-emitter summary (name, enabled, sim target, renderer classes) |
| `GetEmitterTopology` | Four stacks (`EmitterSpawnScript`, `EmitterUpdateScript`, `ParticleSpawnScript`, `ParticleUpdateScript`) with modules in execution order + renderer refs |
| `GetScriptStackTopology` | One stack by `FNiagaraExt_StackItemReference` |
| `GetModuleTopology` / `GetStackInputTopology` | Module metadata + input visibility/editability/dynamic/static-switch flags |

**Stack view-model is the resolution substrate, not a dead-end:**
`FNiagaraExt_StackItemReference` resolves through `UNiagaraStackScriptItemGroup` /
`UNiagaraStackModuleItem` / `UNiagaraStackFunctionInput`
`[VERIFIED: NiagaraExternalSystemEditorUtilities.h:945-1007]`. That is editor view-model
territory, but Epic already wraps it in a stable AICallable surface — UEREMCP must
**internalise** that surface, not re-walk Slate view models.

**Not the only path, but the right agent path:** direct `UNiagaraScript` / `UNiagaraGraph`
exists for script graphs; stack authoring for agents is the External utilities + toolset.

**Runtime:** `GetSystemSummary` + `GetEmitterTopology` on
`/Game/VFX/Spells/Firebolt/Systems/NS_FB_Projectile` returned 6 emitters and ordered
module stacks (e.g. Sparks1 ParticleUpdate: ParticleState → ScaleSpriteSize → Drag →
ScaleColor → CurlNoiseForce → SolveForcesAndVelocity → DynamicMaterialParameters)
`[VERIFIED-RUNTIME: execute_tool_script batch 2026-07-29]`.

**Gap — event handlers:** `GetEmitterTopology` walks only the four named stacks
`[VERIFIED: NiagaraExternalSystemEditorUtilities.cpp:1473-1476]`. Firebolt still has
`ParticleEventScript` entries in compile state and stack issues
(`Event Handler - Source: DeathEvent/...`), but `GetScriptStackTopology` with
`ParticleEventScript` failed ("Script not found") because event groups need a usage
`FGuid` that the public reference API does not expose
`[VERIFIED-RUNTIME: negative probe]`. **Must live in `extensions.niagara.event_handlers`
as a known incomplete area until Epic extends the reference or we propose an internal
walk.**

#### 2. Module inputs (values, dynamic inputs, linked params)

| Call | Role |
|---|---|
| `GetEmitterInputValues` / `GetScriptStackInputValues` / `GetModuleInputValues` | Resolved values, parallel to topology |
| `GetStackInputData` | Single input |
| `GetDynamicInputChain` | Recursive dynamic-input tree when `bIsDynamic` |

Value modes are instanced structs: local literals, `Linked`, `HlslExpression`,
`DataInterface`, `DynamicInput`, `Enum`, `Unsupported`
`[VERIFIED: NiagaraExternalSystemEditorUtilities.h:515-598]`.

**Runtime:** Sparks1 returned 9 module value bundles; ScaleColor had dynamic input
`Scale Alpha` and `GetDynamicInputChain` returned an instanced struct payload
`[VERIFIED-RUNTIME]`.

**Graph mapping:** each module → `node`; each input → `input_pins[]` with
`default_value` for locals; linked/dynamic/DI/HLSL → `extensions.niagara.inputs[pin_id]`
(mode + payload). Do not invent fake data `links` between stack modules for parameter
bindings — bindings are to Niagara parameter names, not peer module pins.

#### 3. Parameter store

- System user params: `GetUserVariables` / `GetSystemSummary.userVariables`
  `[VERIFIED: NiagaraToolset_System.h:289-290]`
- Component overrides: `NiagaraToolset_Component.GetUserVariables` /
  `GetVariable` / `SetVariable` `[VERIFIED: NiagaraToolset_Component.h]`
- Emitter/particle namespaces appear as linked variables and module inputs, not a
  separate "dump entire parameter store" tool.

**Runtime:** Firebolt exposes 18 user vars (`User.Color_*`, `User.Scale_*`,
`User.Velocity_*`, `User.Shape_*`) — ready-made elemental knobs
`[VERIFIED-RUNTIME]`. Probe system accepted `AddUserVariables` for
`User.Color` / `User.Intensity` / `User.Scale` `[VERIFIED-RUNTIME]`.

Underlying type includes `UNiagaraUserRedirectionParameterStore` for User.* redirection
`[UNVERIFIED — not opened this run; Epic toolset abstracts it]`. For implementation, prefer
toolset APIs over hand-walking the store.

#### 4. Renderers

`GetEmitterTopology.renderers` → `{rendererIndex, rendererClass}`;
`GetRendererData` → JSON `propertyValues` including Material, bindings, facing, etc.
`AddRenderer` / `RemoveRenderer` / `SetRendererData` for write
`[VERIFIED: NiagaraToolset_System.h]`.

**Runtime:** Sparks1 sprite renderer material
`/Game/VFX/Spells/Firebolt/Materials/MI_FB_Ex_FlareCore` with Position/Color/Velocity
bindings `[VERIFIED-RUNTIME]`. Firebolt uses Sprite + Mesh + Ribbon across emitters.

#### 5. Data interfaces, curves, mesh sampling

`GetSystemDependencies` returns `UsedDataInterfaces`, `UsedModules`, `UsedDynamicInputs`,
`UsedRenderers` `[VERIFIED: header]`. DI property blobs via stack input
`StackInputData_DataInterface.PropertyValues` or `GetDataInterfaceSchema`.

**Runtime:** Firebolt deps: 24 modules, 2 DIs (`NiagaraDataInterfaceCurve`,
`NiagaraDataInterfaceVector2DCurve`), 5 dynamic inputs, 3 renderer classes
`[VERIFIED-RUNTIME]`.

#### 6. Emitter-level settings

`GetEmitterData.propertyValues` JSON blob of `FVersionedNiagaraEmitterData` fields:
`bLocalSpace`, `bDeterminism`, `RandomSeed`, `SimTarget`, `Importance`,
`CalculateBoundsMode`, `FixedBounds`, scalability, allocation, etc.
`[VERIFIED: NiagaraToolset_System.h:302-315; NiagaraEmitter.h:307-374]`.

`bIsInheritable` is on `UNiagaraEmitter` (asset option), editor-only
`[VERIFIED: NiagaraEmitter.h:791-793]`.

**Runtime:** Sparks1: `bLocalSpace=false`, `SimTarget=CPUSim`,
`CalculateBoundsMode=Dynamic`, FixedBounds ±100 `[VERIFIED-RUNTIME]`.

Warmup / LOD: partially in system `GetSystemData` / emitter scalability overrides;
full enumeration deferred to implementation with schema-driven property lists.

#### 7. Compilation status → `diagnostic.node_id`

| API | Behaviour |
|---|---|
| `GetSystemCompileState` | **Async**; `RunWhenCompileComplete` polls until idle (timeout CVar default **120s**), then collects aggregate + per-script status/events `[VERIFIED: NiagaraToolset_System.cpp:26-29,484-521,544+]` |
| `GetStackIssues` | Same await; issues carry `Location` (`EmitterName`/`ScriptName`/`ModuleName`) + `StackDisplayPath` `[VERIFIED: NiagaraExternalSystemEditorUtilities.h:1072-1117]` |
| `ApplyStackIssueFix` | Fix-style only; waits for post-fix compile |

Compile events carry `NodeGuid` / `PinGuid` (script-graph identity) — map to
`semantic_id` / stack location, **not** engine GUID as contract identity (ADR-0004).

**Runtime:** Firebolt `aggregateStatus=UpToDate`, `bIsCompiling=false`, 0 errors;
18 Info issues with module locations `[VERIFIED-RUNTIME]`. Probe system also UpToDate
after create + AddEmitter `[VERIFIED-RUNTIME]`.

---

### B. Writing

#### 8. Emitters from templates / inheritance

`AddEmitter(System, TemplateEmitter, EmitterName)` required; returns full emitter
topology `[VERIFIED: NiagaraToolset_System.h:462-463]`.

**Runtime:** Added `ProbeBurst` from
`/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst` onto
`/Game/__UeremcpTests/NS_WS07_Probe` — modules and sprite renderer present
`[VERIFIED-RUNTIME]`.

Inheritance: `bIsInheritable` exists on emitter assets; toolset does not expose merge /
override control as a dedicated AICallable. Treat "survive inheritance" as **partial /
unverified for round-trip** until a parent-emitter merge probe is run under
`/Game/__UeremcpTests/`.

#### 9. Module stack mutate

| Op | Tool |
|---|---|
| Add | `AddModule`, `AddSetParametersModule`, `AddSetParameterEntry` |
| Remove | `RemoveModule`, `RemoveSetParameterEntry` |
| Enable | `SetModuleEnabled` |
| Input write | `SetStackInputData` (refuses when `!bIsEditable`) |

**Reorder:** **no AICallable**. Editor has `FNiagaraStackGraphUtilities::MoveModule`
`[VERIFIED: NiagaraStackGraphUtilities.h:324]` but it is **not** on NiagaraToolsets
(56-tool inventory). Gap: reorder requires remove+re-add (lossy for inputs) or a
future proposal / internal C++ call into editor utilities.

#### 10. New module script authoring

**Restricted to composing existing modules** for POC B/C. Toolset has
`FindNiagaraScripts` / `GetNiagaraScriptDigest` / `GetModuleSchemaFromAsset` —
discovery only, no "create Niagara module script graph" tool
`[VERIFIED: NiagaraToolset_Assets.h; 56-tool list]`. Novel HLSL modules are out of
scope; elemental variation is params + materials + emitter composition.

#### 11. Create from scratch vs duplicate template

`CreateNiagaraSystem(AssetName, AssetPath, TemplateSystem)` — **template required**
`[VERIFIED: runtime schema + header]`. Empty `templateSystem.refPath` rejected
`[VERIFIED-RUNTIME: "None is not valid value for property 'TemplateSystem'"]`.

**Honest recommendation:** duplicate-and-modify is the *designed* path (matches
ADR-0004). Use `/Niagara/DefaultAssets/Templates/Systems/MinimalLightweight` (or
project firebolt) as base; add emitters from `/Niagara/DefaultAssets/Templates/Emitters/*`.

**Runtime:** Created `/Game/__UeremcpTests/NS_WS07_Probe` from MinimalLightweight,
saved successfully `[VERIFIED-RUNTIME]`.

#### 12. Compile trigger and await

There is no separate "CompileNow" AICallable on the toolset. Mutations dirty the
system; `GetSystemCompileState` / `GetStackIssues` / `ApplyStackIssueFix` **await**
in-flight compiles via ticker poll (120s default)
`[VERIFIED: NiagaraToolset_System.cpp]`. Underlying engine:
`UNiagaraSystem::RequestCompile` / `WaitForCompilationComplete`
`[VERIFIED: NiagaraSystemViewModel.cpp usages]`.

**Rule for UEREMCP:** never report `*_validated` until async compile-state returns
non-compiling + no errors (and structural checks pass).

#### 13. "Validated" beyond compile

Minimum bar for `created_and_validated` / `modified_and_validated`:

1. `GetSystemCompileState`: `!bIsCompiling`, `!bHasErrors`, Aggregate UpToDate*
2. `GetStackIssues`: `numErrors == 0` (warnings → `created_with_warnings`)
3. Every requested emitter present and `bEnabled` as specified
4. Every emitter that should render has ≥1 renderer with resolvable Material (or
   documented unbound)
5. `GetSystemDependencies` DIs present / no unresolved module scripts
6. Asset saved (`AssetTools.save_assets`)
7. Optional place via `RENiagaraWorkflowTools.place_niagara_system_and_verify` for
   smoke — **not** screenshot-as-validation (POC B10)

---

### C. Semantic layer

#### 14. `create_niagara_effect` specification (additive categories)

Design `specification` (owned later under `schemas/domains/niagara/`) as:

```text
effect_type: enum (open-ended string + known values)
element: fire|water|wind|earth|ice|lightning|arcane|custom
components[]: { role, archetype, enabled, overrides }
user_parameters: { color, scale, intensity, ... }
materials: { role → path | create_spec }
template_system / base_system (optional)
emitters: optional explicit stack plans (advanced)
```

Known `effect_type` values from master prompt (projectile, explosion, beam, …) are
**additive strings**, not a closed schema enum that requires redesign to extend.
New categories add templates in WS-15 library, not schema forks.

Envelope example already uses `create_niagara_effect` + `effect_type: projectile`
`[VERIFIED: schemas/envelope/request.schema.json]`.

#### 15. Minimum reusable emitter archetypes (hand-off to WS-15)

| Archetype | POC B role | Engine starting templates (examples) |
|---|---|---|
| `core` | core / REF | Minimal, SingleLoopingParticle, DirectionalBurst |
| `shell` | flame_shell / Spiral | Mesh burst / UpwardMeshBurst |
| `sparks` | sparks | SimpleSpriteBurst, ConfettiBurst |
| `smoke` | smoke | Fountain, BlowingParticles, project smoke emitters |
| `ribbon_trail` | ribbon_trail | LocationBasedRibbon, DynamicBeam |
| `impact_burst` | impact_burst | OmnidirectionalBurst, SimpleSpriteBurst |

Project prior art: `NS_FB_Projectile` already implements six-emitter projectile with
elemental User colors/scales `[VERIFIED-RUNTIME]` — promote pattern, don't reinvent.

#### 16. What `extensions.niagara` must carry

- Per-stack: `script_usage`, module order indices, `bIsSetParametersModule`
- Per-input: value **mode** + linked variable / HLSL / DI JSON / dynamic chain tree
- Renderers: full `propertyValues` blob (or typed subset + material path)
- Emitter/system property blobs (`GetEmitterData` / `GetSystemData`)
- Event handler stacks (until Epic exposes them on topology)
- Inheritance: parent emitter path, `bIsInheritable`, override flags (when readable)
- Compile: aggregate + per-script + issue Location → our `node_id`/`semantic_id` map
- Elemental: declared user-param contract (`Color`, `Scale`, `Intensity`, …)

---

### D. Verification

#### 17. Headless preview / particle spawn proof

No AICallable "simulate N frames and return particle counts" found in the 56-tool
surface. Practical smoke:

- Compile + stack issues (structural)
- Place component via RE workflow / `NiagaraToolset_Component.SetSystem`
- Optional PIE / viewport — screenshot supplementary only

**Negative finding:** cheap headless particle-count proof is **not** available on the
current toolset. Do not gate POC B on it; gate on compile + structure + save.

#### 18. Performance signals

Not exposed on NiagaraToolsets. Scalability settings appear in emitter/system property
JSON. Runtime GPU/CPU cost / live particle counts: **unavailable** via current
AICallable surface `[VERIFIED: inventory]`. Defer; do not block POC.

---

## NiagaraToolsets inventory (56 tools, source + runtime)

Runtime load confirmed via `list_toolsets` — five classes under `NiagaraToolsets.*`
`[VERIFIED-RUNTIME: 2026-07-29]`.

| Class | Tools (count) | Disposition |
|---|---|---|
| `NiagaraToolset_System` | 46 — create/summary/topology/data/edit/compile/issues | **preserve / internalise**; agent-facing supersede with `niagara.read_graph` / `niagara.submit_graph` / `create_niagara_effect` |
| `NiagaraToolset_Assets` | 3 — discovery, find scripts, digest | preserve |
| `NiagaraToolset_Component` | 4 — SetSystem, user var get/set | preserve; compose for place/verify |
| `NiagaraToolset_Blueprint` | 2 — BP wrappers | defer / optional |
| `NiagaraToolset_Info` | 1 — `UEnum_Info` | preserve |

Full System tool names match WS-02 `docs/audit/raw/plugins/NiagaraToolsets.json`
(56 C++ tools) `[VERIFIED: cross-check]`.

### Exact gaps vs UEREMCP needs

| Gap | Severity | Mitigation |
|---|---|---|
| No module **reorder** tool | High for faithful stack replace | remove+re-add; or internal `MoveModule`; propose Epic/WS-03 helper if reorder becomes hard requirement |
| Event handler stacks not in `GetEmitterTopology` | High for death/impact FX fidelity | `extensions.niagara` + fidelity.lossy_areas; research deeper stack-root walk in Phase 2 |
| Create requires template | None (by design) | Always duplicate-and-modify |
| No new module script authoring | Accept for POC | Compose Epic modules only |
| No headless particle sim / perf counters | Medium | Structural validation + optional place |
| Inheritance merge controls not AICallable | Medium | Document; probe later |
| REAgentTools Niagara = place/params only | Expected | Batch Epic tools via `execute_tool_script` |

### Batching

`$RAT/Docs/NIAGARA_BATCHING.md` + `ProgrammaticToolset.execute_tool_script` confirmed
working for multi-step create/inspect/compile/save in **one** MCP hop
`[VERIFIED-RUNTIME]`. UEREMCP `execute_plan` should compose the same way (WS-05), with
primitives hidden via `SetNameFilters` (ADR-0002).

---

## Elemental parameterization (fire / water / wind / earth / ice)

Firebolt pattern to generalise:

- **User.Color_*** LinearColors per emitter role
- **User.Scale_*** / **User.Velocity_*** / **User.Shape_*** floats
- Materials per renderer (WS-08)
- Module choice for forces (CurlNoise vs gravity vs vortex) for wind/earth feel

POC C ice variation = duplicate system + retarget materials + rewrite User colors +
optional add crystalline emitter archetype — **one** envelope request.

---

## Negative findings / limitations (summary)

1. Event/Death handler module stacks not readable via public topology API as used.
2. No ReorderModule on toolset.
3. No from-scratch empty system (template mandatory).
4. No module-graph authoring; composition ceiling only.
5. No headless particle-count validation tool.
6. Stack resolution depends on editor view-model (`UNiagaraStack*`) — editor-only plugin
   required (acceptable for UEREMCP editor plugin).
7. `GetEmitterData` / `GetRendererData` / `GetSystemData` return opaque JSON strings —
   must parse carefully; schema endpoints exist for property discovery.
8. Phase 1 gates: do **not** implement `Plugins/UEREMCP/Source/UeremcpNiagara/**` or
   `schemas/domains/niagara/**` until Wave 1 closes.

---

## Mapping sketch — module stack → graph.schema.json

```json
{
  "graph_type": "NiagaraModuleStack",
  "graph_id": "NS_FB_Projectile::Sparks1::ParticleUpdateScript",
  "nodes": [
    {
      "node_id": "n0",
      "semantic_id": "Sparks1/ParticleUpdate/ParticleState",
      "node_class": "UNiagaraStackModuleItem",
      "semantic_type": "niagara_module",
      "title": "ParticleState",
      "properties": {
        "module_script": "/Niagara/Modules/Update/Lifetime/ParticleState",
        "enabled": true,
        "stack_index": 0
      },
      "input_pins": [/* from GetModuleTopology + values */]
    }
  ],
  "links": [
    {"from_node": "n0", "from_pin": "exec_out", "to_node": "n1", "to_pin": "exec_in", "kind": "exec"}
  ],
  "fidelity": {
    "round_trip_supported": false,
    "lossy_areas": ["event_handler_stacks", "module_reorder_without_readd", "script_graph_internals"]
  },
  "extensions": {
    "niagara": {
      "script_usage": "ParticleUpdateScript",
      "emitter_name": "Sparks1",
      "inputs": {}
    }
  }
}
```

---

## Implementation plan (conditional on Phase 1 closure)

Gate: WS-02 audit accepted, WS-03 plugin compiles with AICallable, WS-05 envelope
validator green, WS-11 harness can run one editor test.

1. **Schemas (WS-07 owned):** `schemas/domains/niagara/graph-ext.schema.json`,
   `create_niagara_effect.specification.schema.json` — extend `specification` only.
2. **Module `UeremcpNiagara`:** thin orchestrator over Epic NiagaraToolsets +
   `execute_tool_script` batching; hide primitives with `SetNameFilters`.
3. **`niagara.read_graph`:** summary → per-emitter topology + input values +
   dependencies + compile/issues in one response (`response_detail` tiers).
4. **`create_niagara_effect`:** template duplicate → AddEmitter×N → SetRendererData /
   materials (WS-08) → AddUserVariables → await compile → validate → save under
   allowed roots; tests only under `/Game/__UeremcpTests/`.
5. **POC C:** variation op reusing structural plan + param/material overrides;
   hand template to WS-15.
6. **Tests:** unit (JSON mapping), editor integration (create/read/compile await/
   validation statuses).
7. **Proposals if needed:** WS-01 for any `graph.schema.json` enum tweak; WS-03 if
   needing exported MoveModule helper; never silent ADR fork.

---

## Deliverables checklist

- [x] Research answers A–D with verification tags
- [x] Explicit ADR-0004 / R-05 verdict (**no fork**)
- [x] 56-tool inventory + gaps
- [x] Runtime probes under `/Game/__UeremcpTests/` (+ read-only Firebolt)
- [x] Emitter archetype list for WS-15
- [x] Implementation plan gated on Phase 1
- [ ] Complete read mapper into `graph.schema.json` — **Phase 2 implementation**
- [ ] POC B / POC C — **Phase 2**
- [ ] `schemas/domains/niagara/` — **Phase 2** (not edited this run)

## Runtime artifacts (editor project, not this repo)

- `/Game/__UeremcpTests/NS_WS07_Probe` — created from MinimalLightweight, +ProbeBurst
  emitter, User.Color/Intensity/Scale, saved
  `[VERIFIED-RUNTIME]`

## RE Inspect automation rebuild attempt (2026-07-30)

- Orch contains `0754f7b` as patch-equivalent cherry-pick `e9bc110` (stable patch ID
  `aa85927a769f69c9709073adb88a7dc6b4e592f9`). RE was repointed to the orch junction;
  no Unreal Editor or Live Coding process was active.
- The orch `REEditor Win64 Development -Module=UeremcpNiagara -WaitMutex
  -NoHotReloadFromIDE` build stopped in rules evaluation because
  `UeremcpMaterial.Build.cs` depends on a module named `Editor`
  `[VERIFIED-RUNTIME: UBT 2026-07-30, "Could not find definition for module 'Editor'"]`.
- A fallback build with RE junctioned to `ws-07-niagara` reached C++ compilation but
  failed in the Inspect implementation: `FNiagaraExt_StackInputData_DataInterface`
  has no `DataInterfaceClass` member
  `[VERIFIED-RUNTIME: MSVC C2039 in UeremcpNiagaraInspect.cpp:82/84]`.
  The UE 5.8 struct exposes `PropertyValues` only
  `[VERIFIED: Engine/Plugins/FX/Niagara/Source/NiagaraEditor/Public/NiagaraExternalSystemEditorUtilities.h:574-581]`.
  UHT also reported the pre-existing cross-workstream first-include error in
  `UeremcpTemplatesModule.cpp`.
- `UEREMCP.Niagara.Inspect.PathGuard` and
  `UEREMCP.Niagara.Inspect.NS_WS07_Probe` were not run: no binary containing the
  target Inspect implementation was produced. RE's plugin junction was restored to
  `UEREMCP-ws01/Plugins/UEREMCP`.

## RE Create automation verification attempt (2026-07-30)

- Orch contains Create commit `ad2d517` as patch-equivalent cherry-pick `4e48fd0`
  (stable patch ID `a38ffd7c7311f5cc28178a7e32f4f043a977a62b`). The RE plugin
  junction targeted `UEREMCP-ws01/Plugins/UEREMCP` throughout the final build and
  automation attempts. No Live Coding console process was active, so no Live Coding
  shutdown was required.
- `REEditor Win64 Development -Module=UeremcpNiagara -WaitMutex
  -NoHotReloadFromIDE` succeeded after the UE 5.8 compile/link corrections
  `01d121e` and `6ade91c`
  `[VERIFIED-RUNTIME: UBT Result: Succeeded, 2026-07-30]`.
- `UEREMCP.Niagara.Inspect` was launched through the RE shipping harness, but the
  editor exited before test discovery because registered module `UeremcpMaterial`
  had no loadable binary
  `[VERIFIED-RUNTIME: editor_UEREMCP_Niagara_Inspect_20260730_013028.log]`.
- Rebuilding that prerequisite reached a new cross-workstream Core compile failure:
  `UeremcpMutatingDispatchTests.cpp` calls nonexistent
  `FString::ReplaceCharWith` and supplies too few arguments to `FString::Printf`
  `[VERIFIED-RUNTIME: UBT C2039/C7595, 2026-07-30]`.

| Filter / test | Result | Evidence |
|---|---|---|
| `UEREMCP.Niagara.Inspect` | **BLOCKED — 0 tests run** | Editor plugin load stopped at missing `UeremcpMaterial` binary |
| `UEREMCP.Niagara.Inspect.PathGuard` | **NOT RUN** | Filter did not reach automation discovery |
| `UEREMCP.Niagara.Inspect.NS_WS07_Probe` | **NOT RUN** | Filter did not reach automation discovery |
| `UEREMCP.Niagara.Create` | **BLOCKED — 0 tests run** | Same full-plugin load prerequisite; not launched after deterministic Core compile failure |
| `UEREMCP.Niagara.Create.PathGuard` | **NOT RUN** | No loadable full plugin binary |
| `UEREMCP.Niagara.Create.DryRun` | **NOT RUN** | No loadable full plugin binary |

No Create mutation ran and no asset-validation status is claimed. The honest outcome
for this tip is **failed validation gate / runtime verification blocked**, not
`created_and_validated`.
