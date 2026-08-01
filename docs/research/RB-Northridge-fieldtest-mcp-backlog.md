# RB-Northridge — Environment MCP field-test backlog

**Status:** requirements backlog (docs only)  
**Source session:** Northridge starter fantasy region (landscape, materials, beach/ocean, roads, 3 cities, castle, trees) via Environment MCP / agents  
**Plugin:** `Plugins/UEREMCP` (UEREMCP Environment + Core IntentRouter)  
**Deliverable intent:** API shapes + acceptance tests so agents stop fighting the MCP while building a starter region  
**Convention:** README / toolset comments already cite `docs/research/RB-*.md` (folder was missing in this checkout; this file establishes it)

Related code (cite lightly):

| Area | Touchpoints |
|------|-------------|
| Staged wipe | `UeremcpEnvironmentService.cpp` → `DestroyOwnedEnvironmentActors`, `Build()` |
| Staging entry | `UeremcpEnvironmentToolset.cpp` → `DispatchEnvAction` / `ApplyStageIncludes` |
| Spec / scale | `ParseBuildSpec`, `FUeremcpEnvironmentBuildSpec` (`ScaleXY`/`ScaleZ` defaults 100) |
| Catalog lies | `Content/IntentRouter/operation_catalog.json` (`/Game/Meshes/SM_Pine`, lake/ocean use_when) |
| Intent UX | `UeremcpIntentRouter.cpp` (`GetStarted`, `ResolveIntent`) |
| Validate gates | `FUeremcpEnvironmentService::Validate` |
| Mesh primitives | `SubmitMeshOps` / `UeremcpMeshOps.cpp`; honesty notes on `UeremcpEnvironmentToolset.h` |
| Materials | `UeremcpMaterialToolset` (VFX/master only — no landscape mat tool) |
| Schema Phase 2 | `Content/Schemas/domains/environment/build_environment.schema.json` (`hydrology.lake` / `ocean`) |

---

## A. Executive summary — why starter scenes fail

Agents can spawn individual Environment pieces, but they cannot reliably **compose** a starter region.

1. **Staging destroys prior work.** `CreateLandscape` → `CreateWaterBody` → `ScatterFoliage` each call `Build()`, which begins with `DestroyOwnedEnvironmentActors(World)` using the empty prefix (`UEREMCP_*`). River/foliage stages wipe the landscape (and each other). Docs claim foliage `vegetation.group` is additive; `Build()` never uses that prefix for selective replace.
2. **Routing invents assets and over-promises water.** Catalog examples / suggestions point at `/Game/Meshes/SM_Pine` and snow/ice-wall paths that are not in the project. `CreateWaterBody` advertises lake/ocean in `use_when` while schema marks them Phase 2 and implementation only spawns rivers (`body_type` ignored).
3. **Scale is easy to get wrong and hard to detect.** Nonuniform `terrain.scale_xy` vs `terrain.scale_z` is applied as actor relative scale; docs warn about needle `scale_z` after the fact. Agents “fixed” spiky terrain multiple times. `ValidateEnvironment` only checks a coarse `non_flat` height range — not uniform scale or flat-area %.
4. **Quality paths are honest in comments, soft in behavior.** Missing foliage mesh → cube with a warning (`prefer_real` helps only if forest is actually gated). `SubmitMeshOps` cone+cylinder “trees” succeed despite hero-art honesty notes. Project fallbacks (e.g. PCG trees observed in-session) are never suggested. No `CreateLandscapeMaterial`; expression-graph landscape mats silently ship with BaseColor disconnected.
5. **Validate is a MountainRiverRain checklist, not scene health.** Floating foliage, water/terrain disconnect, beaches-as-cubes, and “looks like a starter fantasy map” are invisible. Dirty untitled maps reject `BuildEnvironment` map switches, multiplying round-trips. After wipes, agents abandon procedural paths and hand-place props.

**Bottom line:** the MCP optimizes for single-subsystem demos (mountain + river + rain), not additive multi-step region construction with resolved assets and hard realism gates.

---

## B. Prioritized backlog

### P0 — ship-blockers for any multi-step Environment agent

#### P0-1. Additive staging (stop `UEREMCP_*` wipe on every stage)

| | |
|--|--|
| **Problem** | Staged `create_landscape` → `create_water_body` → `scatter_foliage` each destroy all owned actors. Field test: river/foliage wiped landscape. |
| **Proposed API** | Keep tool names; change semantics. Optional explicit replace: `"options":{"replace_owned":"stage"\|"all"\|"none"}` (default **`stage`** for stage tools, **`all`** only for full `build_environment`). Stage destroy prefixes: landscape→`UEREMCP_Landscape*`, water→`UEREMCP_River*`, foliage→`Spec.FoliageActorLabel()`, etc. Reject `replace_owned=all` on stage tools unless `options.confirm_wipe=true`, with recovery text listing destroyed labels. |
| **Agent UX** | “Add water” never deletes terrain. Response includes `change_manifest.replaced_owned_actors` + `preserved_owned_actors`. |
| **Acceptance tests** | Automation: CreateLandscape → CreateWaterBody → Assert landscape actor still present; ScatterFoliage → both landscape + river present. Negative: `replace_owned=all` without confirm → `rejected`. |
| **Touchpoints** | `DestroyOwnedEnvironmentActors` (~L538), `Build()` call at ~L1391, `DispatchEnvAction` / `ApplyStageIncludes` in `UeremcpEnvironmentToolset.cpp`, foliage label helper `FoliageActorLabel()`. |

#### P0-2. `plan_environment` / ResolveIntent returns filled executable plan + asset resolution

| | |
|--|--|
| **Problem** | ResolveIntent / catalog suggested nonexistent `/Game/Meshes/SM_Pine` and snow/ice_wall paths for a temperate fantasy ask; agents burned turns chasing ghosts. |
| **Proposed API** | New or upgraded tool: `plan_environment` (or `ResolveIntent` mode=`plan_environment`). |

```json
{
  "protocol_version": "1.0",
  "action": "plan_environment",
  "specification": {
    "intent": "starter fantasy region: hills, coast, roads, 3 cities, castle, trees",
    "quality": "realistic",
    "seed": 4471,
    "destination_level_path": "/Game/__UeremcpPoc/Northridge/Northridge"
  }
}
```

**Response sketch:**

```json
{
  "status": "no_change_required",
  "plan": {
    "steps": [
      {"action": "create_landscape", "request_json": { "...filled...": true }},
      {"action": "create_water_body", "request_json": { "...": true }, "unsupported": ["ocean"], "degrade_to": "river_or_reject"},
      {"action": "scatter_foliage", "request_json": {"specification":{"biome":{"mesh_path":"<resolved>"}}}}
    ]
  },
  "asset_resolution": {
    "resolved": [{"role":"tree_mesh","path":"/Game/.../PCG_Tree_01"}],
    "missing": [{"role":"castle_kit","searched":["*Castle*"],"hint":"import or PlaceStructures kind unsupported"}],
    "unsupported_capabilities": ["hydrology.ocean", "beach", "roads", "city_layout"]
  },
  "round_trip_budget": 5
}
```

| | |
|--|--|
| **Agent UX** | Tell goal → get filled `request_json` steps + missing/unsupported list. Never invent `/Game/Meshes/...` without AssetRegistry hit. |
| **Acceptance tests** | Fantasy-region intent must not emit `/Game/Meshes/SM_Pine` unless that package exists. Missing assets appear under `asset_resolution.missing`, not inside executable mesh_path. Catalog example_request paths must either exist in fixture project or be marked `example_only`. |
| **Touchpoints** | `UeremcpIntentRouter.cpp`, `operation_catalog.json` (ScatterFoliage example ~L985), `UeremcpNextActions.cpp`, optional AssetRegistry probe helper. |

#### P0-3. Hard uniform scale + flatness metrics on create_landscape

| | |
|--|--|
| **Problem** | `scale_xy=300` vs `scale_z=100` (and high `scale_z`) produced needle/spiky terrain; agents rediscovered docs too late. |
| **Proposed API** | In `ParseBuildSpec` / `Build`: if `scale_xy != scale_z` (tolerance e.g. 1%) → **reject** unless `terrain.allow_nonuniform_scale=true`. Cap `scale_z` ≤ 150 unless `terrain.allow_extreme_scale_z=true`. Always return: |

```json
"structural_metrics": {
  "actor_scale": {"x": 100, "y": 100, "z": 100},
  "uniform_scale": true,
  "height_range_cm": 12345.0,
  "flat_area_pct": 18.2,
  "slope_p50_deg": 12.4,
  "slope_p95_deg": 41.0
}
```

| | |
|--|--|
| **Agent UX** | Bad scale fails fast with “set both to 100; raise max_altitude_m for taller peaks” (align with toolset comment). |
| **Acceptance tests** | `scale_xy=300,scale_z=100` → `rejected`. Uniform 100 → created + `uniform_scale=true`. Flat profile → `flat_area_pct` above threshold reported; mountains profile → `flat_area_pct` below gate when required. |
| **Touchpoints** | `ParseBuildSpec` scale reads (~L682), `SetActorRelativeScale3D` (~L1413), `UeremcpEnvironmentToolset.h` scale_z docs, schema `terrain.scale_*`. |

#### P0-4. Scene-health ValidateEnvironment (not only MRR gates)

| | |
|--|--|
| **Problem** | Validate passed/failed on landscape+river+forest+rain checklist; missed floating foliage, water/terrain disconnect, flat%, uniform scale. |
| **Proposed API** | Extend `validate_environment` specification: |

```json
{
  "action": "validate_environment",
  "specification": {
    "profile": "starter_region",
    "gates": {
      "require_uniform_landscape_scale": true,
      "max_flat_area_pct": 35,
      "max_foliage_snap_error_cm": 50,
      "max_water_terrain_gap_cm": 200,
      "min_water_overlap_samples_ok_pct": 90,
      "forbid_basicshape_foliage": true,
      "require_rain": false
    }
  }
}
```

| | |
|--|--|
| **Agent UX** | One health call after build; failed gates name the fix tool. Starter profile must not require rain. |
| **Acceptance tests** | Fixture with elevated HISMC instances → fail snap gate. Nonuniform landscape scale → fail. Cube foliage under `forbid_basicshape_foliage` → fail. MRR profile remains available for legacy. |
| **Touchpoints** | `Validate` (~L2100+), `Inspect`, structural_metrics writers in `Build`. |

#### P0-5. Realism policy — reject primitive foliage for realistic goals

| | |
|--|--|
| **Problem** | Cone+cylinder trees via `SubmitMeshOps` succeeded despite honesty notes; cubes used as beach/trees; quality ask ignored. |
| **Proposed API** | Thread `specification.quality` / `fallback_policy` into mesh + foliage: |

```json
"specification": {
  "quality": "realistic",
  "fallback_policy": "prefer_real"
}
```

- `quality=realistic|hero|reference` + foliage → reject BasicShapes and reject `submit_mesh_ops` results as `biome.mesh_path` unless `options.allow_primitive_foliage=true`.
- `submit_mesh_ops` with `quality=realistic` → `rejected` with next_action pointing at import / project catalog meshes.

| | |
|--|--|
| **Agent UX** | Soft warnings are insufficient; status must be `rejected` / `failed_validation`. |
| **Acceptance tests** | Scatter with missing mesh + prefer_real → reject (already partially true). Scatter with Cube path + realistic → reject. SubmitMeshOps tree icon + realistic → reject. allow_approximate / blockout → allow with `approximated=true`. |
| **Touchpoints** | `CheckFallbackPolicy`, foliage cube fallback (~L1524), `SubmitMeshOps`, toolset honesty comments (~L138–162). |

#### P0-6. GetStarted capability probe + project fallbacks

| | |
|--|--|
| **Problem** | Blender PolyHaven/Sketchfab disabled with no fallback catalog; HTTP PolyHaven worked; PCG trees existed in project but were never suggested. Dirty untitled maps blocked BuildEnvironment. |
| **Proposed API** | Expand `GetStarted` (detail=`environment` \| `capabilities`): |

```json
{
  "action": "get_started",
  "specification": { "detail": "environment" }
}
```

Payload includes: Water plugin present, ocean/lake Phase 2 unsupported, AssetRegistry hits for `*Tree*`,`*Pine*`,`PCG_Tree*`, map dirty/untitled state + recovery (`save_as` path or `build_in_current_map`), external MCP (Blender/PolyHaven) availability note if detectable, recommended `destination_level_path`.

| | |
|--|--|
| **Agent UX** | First call answers “what can I use in *this* project?” before planning. |
| **Acceptance tests** | Fixture with known tree mesh → appears in `project_fallbacks.trees`. Untitled dirty map → `map_state.dirty=true` + recovery string. Ocean listed under `unsupported`. |
| **Touchpoints** | `FUeremcpIntentRouter::GetStarted`, dirty-map reject in `Build` (~L1334–1341), catalog `use_when` for lake/ocean. |

---

### P1 — required for “looks like a region,” not just heightmap+HISMC

#### P1-1. `CreateLandscapeMaterial` (height/slope layers + texture binds)

| | |
|--|--|
| **Problem** | No landscape material tool; agents used expression graphs → BaseColor disconnected; silent visual failure. |
| **Proposed API** | `UeremcpMaterialToolset.CreateLandscapeMaterial` or Environment-owned helper: |

```json
{
  "action": "create_landscape_material",
  "target": { "asset_path": "/Game/__UeremcpPoc/Northridge/M_Northridge_Landscape" },
  "specification": {
    "layers": [
      {"name": "Grass", "height_min": 0.0, "height_max": 0.45, "slope_max_deg": 25, "albedo": "/Game/.../T_Grass"},
      {"name": "Rock", "height_min": 0.4, "height_max": 1.0, "slope_min_deg": 20, "albedo": "/Game/.../T_Rock"},
      {"name": "Sand", "height_min": 0.0, "height_max": 0.15, "mask": "coast_band", "albedo": "/Game/.../T_Sand"}
    ],
    "assign_to_landscape_label": "UEREMCP_Landscape"
  }
}
```

Must verify BaseColor (or Landscape Layer Blend → BaseColor) connected before success; otherwise `failed_validation`.

| | |
|--|--|
| **Agent UX** | One call yields assigned, compiling landscape MI; no raw expression-graph editing. |
| **Acceptance tests** | Created material has connected BaseColor; landscape `LandscapeMaterial` set; capture shows non-default shading vs unassigned control. |
| **Touchpoints** | New tool beside `CreateMasterMaterial` / `CreateVfxMaterial`; assign in Environment service after create. |

#### P1-2. Clear recovery when wipe is requested / dirty map friction

| | |
|--|--|
| **Problem** | Dirty untitled maps → BuildEnvironment `rejected`; many round-trips. After accidental wipes, no recovery plan. |
| **Proposed API** | On dirty reject, return `recovery.request_json` for save-as under `/Game/__UeremcpPoc/...` or `options.allow_build_in_dirty_map` (explicit). On wipe, return `undo_hint` + last revision tag. |
| **Acceptance tests** | Dirty untitled → rejected with machine-readable `recovery`; after save-as path used → build proceeds. |
| **Touchpoints** | `Build` map lifecycle (~L1328–1354). |

#### P1-3. Catalog honesty pass (examples must resolve or be flagged)

| | |
|--|--|
| **Problem** | `operation_catalog.json` teaches bad paths and lake/ocean for a river-only tool. |
| **Proposed API** | Catalog fields: `"example_assets_verified": false`, remove lake/ocean from CreateWaterBody `use_when` until implemented; add `unsupported_intents`. CI: `tools/check_operation_catalog.py` asserts example mesh paths exist or are prefixed `EXAMPLE_ONLY:`. |
| **Acceptance tests** | Catalog checker fails on `/Game/Meshes/SM_Pine` in this project. |
| **Touchpoints** | `operation_catalog.json`, `check_operation_catalog.py`. |

#### P1-4. Structures beyond ice/barrier boxes (cities / castle / roads — minimal)

| | |
|--|--|
| **Problem** | Castle/cities/roads required manual prop placement; `PlaceStructures` only `ice_wall_ring\|barrier_wall\|box_along_river`. |
| **Proposed API** | P1 kinds: `settlement_markers` (labeled points + simple footprints), `road_spline` (deck mesh or landscape paint stub), `castle_keep_blockout` **only** when `quality=blockout`. Realistic → require kit paths from asset_resolution. |
| **Acceptance tests** | Unknown kind still rejected; new kinds place ≥1 actor with stable labels; Validate counts them under starter profile. |
| **Touchpoints** | `IsSupportedStructureKind`, PlaceStructures path in `Build`, schema `structures`. |

---

### P2 — coast / ocean / polish (do not block additive staging + plan)

#### P2-1. Beach / ocean Phase 2 (real or hard-reject)

| | |
|--|--|
| **Problem** | Beach/ocean unsupported → agents placed cube beaches. |
| **Proposed API** | Either implement `hydrology.ocean` / `coast.beach_band` **or** reject with `on_unsupported=fail` (default) and never suggest cubes. Partial mode returns explicit unsupported list without spawning BasicShapes as coastline. |
| **Acceptance tests** | `hydrology.ocean` without implementation → `rejected` or `partially_completed` with empty beach actors; never Cube coastline under `quality=realistic`. |
| **Touchpoints** | schema Phase 2 lake/ocean; `CreateWaterBody` docs (`body_type` ignored). |

#### P2-2. External asset MCP fallback catalog

| | |
|--|--|
| **Problem** | Blender PolyHaven/Sketchfab disabled; no in-UE fallback list. |
| **Proposed API** | `list_project_mesh_catalog` (AssetRegistry query) + optional `suggested_imports[]` URLs for PolyHaven when HTTP available — not a silent empty. |
| **Acceptance tests** | Disabled Blender MCP → GetStarted still returns project mesh hits. |

#### P2-3. Roads / city layout as first-class Environment blocks

| | |
|--|--|
| **Problem** | Starter region needs connectivity between settlements; out of current Environment scope. |
| **Proposed API** | `layout.settlements[]` + `layout.roads[]` in build spec → markers + splines; full traffic/AI later. |
| **Acceptance tests** | Three settlements + connecting splines present after one Build/ExecutePlan. |

---

## C. Must-have new/changed tools (sketch envelopes)

### 1. `plan_environment` / improved ResolveIntent

See **P0-2**. Must emit filled `request_json`, `asset_resolution`, `unsupported_capabilities`, and a `round_trip_budget`.

### 2. Additive staging OR reject staged wipe

See **P0-1**. Preferred: additive by default. Alternative interim: stage tools **reject** if other `UEREMCP_*` actors exist and `replace_owned` omitted — force agents onto single `build_environment` or explicit wipe. Clearer than silent destruction.

### 3. Hard uniform scale + flatness in create_landscape response

See **P0-3**. Metrics always on; reject nonuniform by default.

### 4. `CreateLandscapeMaterial`

See **P1-1**. Envelope above; success ⇒ connected BaseColor + optional assign.

### 5. Scene health validate

See **P0-4**. Profile `starter_region` vs legacy `mountain_river_rain`.

### 6. GetStarted capability probe + project fallbacks

See **P0-6**.

### 7. Realism policy

See **P0-5**. Enforce in ScatterFoliage + SubmitMeshOps + Validate.

---

## D. Acceptance scenario — “Northridge starter” one-shot

**Goal:** Temperate fantasy starter region: non-needle landscape, layered landscape material, water (river acceptable if ocean unsupported and explicitly degraded), foliage from **real** project meshes (e.g. PCG trees when present), three settlement markers + castle footprint, roads as splines or explicit unsupported, no BasicShape coastline/trees when `quality=realistic`.

### Round-trip budget

**≤ 5 MCP round trips after a successful `plan_environment`** (plan itself is trip 0 or counted separately as trip 1 of ≤ 6 total including plan).

Grounding: today agents need many trips (discover → wrong mesh → wipe → rebuild → cube beach → validate that lies). Target sequence:

| # | Call | Must achieve |
|---|------|----------------|
| 0/1 | `get_started` (detail=environment) *or* folded into plan | Capabilities + project tree fallbacks + map dirty state |
| 1 | `plan_environment` | Filled steps; missing/unsupported listed; no invented paths |
| 2 | `build_environment` **or** `ExecutePlan` of plan steps (additive) | Terrain + water + foliage + settlement/castle markers; no inter-stage wipe |
| 3 | `create_landscape_material` (+ assign) | Connected BaseColor; applied to landscape |
| 4 | `validate_environment` (profile=`starter_region`) | Gates below green |
| 5 | `capture_world_frames` | Human-review artifact (not a gate) |

If plan embeds material create inside ExecutePlan, budget can drop to **≤ 3 after plan** (execute → validate → capture). **≤ 5** is the acceptance ceiling.

### Checklist (all must pass)

- [ ] No `DestroyOwnedEnvironmentActors` full wipe between planned stages
- [ ] `uniform_scale=true`; `flat_area_pct` within gate for chosen terrain profile
- [ ] Foliage mesh paths exist and are not `/Engine/BasicShapes/*`
- [ ] No cone/cylinder-only trees when `quality=realistic`
- [ ] Landscape material BaseColor connected and assigned
- [ ] Water present (river OK) **or** ocean explicitly in `unsupported` with no cube beach
- [ ] ≥ 3 settlement labels + 1 castle/keep label (blockout OK if declared)
- [ ] `validate_environment` profile `starter_region` → success status
- [ ] Total mutating Environment calls after plan ≤ 5 (ideally 1–2)

### Suggested automation test name

`UeremcpEnvironment.NorthridgeStarter.OneShotPlanExecuteValidate`

---

## E. Non-goals / out of scope

- Rebuilding the Northridge Unreal level contents in this doc task (docs/requirements only).
- Full open-world city generation, traffic AI, or narrative quests.
- Implementing real ocean/beach hydrodynamics in P0 (hard-reject or explicit degrade is enough until P2).
- Replacing human screenshot review as the sole beauty gate (structural gates first; BACKLOG 5.8 stands).
- Making Blender/PolyHaven/Sketchfab MCP mandatory; in-project AssetRegistry fallback is the P0 requirement.
- AlphaBrush landscape sculpting (already documented unavailable).
- Expanding structure kinds to full modular castle kits (P1 markers/blockout only unless kits resolve).
- Changing protocol_version or abandoning ADR-0003 envelopes.
- Force-pushing agents to Epic primitive toolsets for Environment goals (GetStarted policy stays: prefer Ueremcp*).

---

## Implementation order (suggested)

1. **P0-1** additive staging (unblocks all composition)  
2. **P0-3** scale reject + metrics (stops silent needle terrain)  
3. **P0-2** + **P0-6** plan + GetStarted asset truth  
4. **P0-5** realism rejects  
5. **P0-4** validate scene health  
6. **P1-1** CreateLandscapeMaterial  
7. **P1-3** catalog honesty CI  
8. P1-2 / P1-4 / P2.* as capacity allows  

---

## Appendix — field-test failure → backlog map

| # | Session failure | Backlog |
|---|-----------------|---------|
| 1 | Staged create wipe | P0-1 |
| 2 | ResolveIntent bad paths | P0-2, P1-3 |
| 3 | Nonuniform / needle scale | P0-3 |
| 4 | Beach/ocean → cubes | P2-1, P0-5 |
| 5 | MeshOps trees / PCG not suggested | P0-5, P0-6 |
| 6 | No landscape material tool | P1-1 |
| 7 | Weak ValidateEnvironment | P0-4 |
| 8 | External MCP down, no fallback | P0-6, P2-2 |
| 9 | Dirty untitled map friction | P1-2, P0-6 |
| 10 | Manual props after unsafe procedural | P0-1, P1-4, D scenario |
