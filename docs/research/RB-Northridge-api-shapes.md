<!-- Imported from ueremcp_fieldtest/docs/ so it is tracked. Do not edit the
     original; it is outside version control and will be lost. -->

## Status against main — 2026-07-31

Checked item by item rather than assumed. `main` is ahead of the deploy
worktree (`71c4506`), so **none of the DONE rows below were live during the
Northridge sessions that produced this document.**

| Item | Status | Where |
|---|---|---|
| MCP-006 ScatterFoliage mutates heightmap | **Done** | staged placement now rejects on hash mismatch |
| MCP-015 Additive staging | **Done** | `c3ac0dd` — per-stage destroy prefixes |
| MCP-017 Scale metrics | **Done** | `6edf07e` — uniform gate + slope p50/p95/max + flat_area_pct |
| MCP-005 Batch guidance | **Done** | `8bcf34d` — fan-out example, `batch_hint`, id-vs-action stated |
| MCP-007 Describe slim | **Partial** | `e96bee7` — contract inline on `next_actions`; no slim mode or ETag |
| MCP-004 Landscape layer paint | **Partial** | `CreateLandscapeMaterial` authors the blend; weight PAINTING not applied |
| MCP-016 plan_environment | **Partial** | phantom paths removed; no AssetRegistry probe |
| MCP-011 Error contracts | **Partial** | rejections state the recipe; no machine-readable `next_args` |
| MCP-001 Multi-water without overwrite | Open | `body_type` still ignored; river only |
| MCP-002 SnapActorsToLandscape | Open | |
| MCP-003 ClearFoliageInVolumes | Open | |
| MCP-008 ImportMeshForWorld one-shot | Open | |
| MCP-009 Weather one-shot | Open | `AttachWeather` exists; surfacing only |
| MCP-010 PlacePrefabOnLandscape | Open | |
| MCP-012 Foliage species discovery | Open | needs the same AssetRegistry probe as MCP-016 |
| MCP-013 Watch server reliability | Open | not UEREMCP-owned |
| MCP-014 SemanticSearch auth | Open | not UEREMCP-owned |
| MCP-018 Capture preference | Open | docs-only |

**Two the document is right to weight highest, and I agree:**

MCP-006 was the worst and is fixed here. A staged call that places on terrain
without rebuilding it still recomputed the heightmap from whatever spec it was
handed, then placed against those computed heights. Any drift in seed, profile,
size or `scale_z` produced a *different surface* than the level actually has —
every actor at the wrong Z. That is the floating castle and huts, and the
changed hash. It now refuses, naming both hashes and the recipe.

MCP-001 is the largest remaining honesty debt: the catalog advertises lake and
ocean, `body_type` is never read, and agents worked around it with SceneTools
class spawns. Fixing the catalog text is a five-minute change; implementing
`AWaterBodyLake` / `AWaterBodyOcean` is not, and the text should stop promising
until it is.

---

# MCP Backlog — API Shapes & Acceptance Tests

**Source:** Northridge fieldtest sessions (2026-07-31) + live UEREMCP schemas  
**Companion report:** [`MCP_Field_Report_Northridge.md`](./MCP_Field_Report_Northridge.md)  
**Prior research:** `Plugins/UEREMCP/docs/research/RB-Northridge-fieldtest-mcp-backlog.md` (staging wipe / plan_environment — folded here as MCP-015 / MCP-016)

IDs are stable for tracking. Priorities: **P0** ship-blockers for additive region agents; **P1** fidelity; **P2** polish.

All new UEREMCP tools use ADR-0003 envelopes unless noted. Epic primitive toolsets stay internal where a goal tool exists.

---

## Verdict: why agents spam calls

| Factor | Share | Action |
|--------|------:|--------|
| Missing goal APIs | ~40% | MCP-001…006, 008–010 |
| Destructive staging / overwrite | ~25% | MCP-001, MCP-015 |
| Discoverability / huge describe | ~20% | MCP-007, MCP-016 |
| Late ExecutePlan / instruction | ~15% | MCP-005 guidance + GetStarted |

Approximate transcript evidence: ~331 `call_tool`, ~61 `describe_toolset`, ~91 `execute_tool_script`, ~10 `ExecutePlan`, ~37 `CreateWaterBody`, ~35 `ScatterFoliage`.

---

## MCP-001 — Multi-water-body create without overwrite

| | |
|--|--|
| **Priority** | P0 |
| **Title** | CreateWaterBodies / honor body_type; unique labels |
| **Problem** | `CreateWaterBody` is river-only; `body_type` ignored (lake/ocean → river). Second call overwrites `UEREMCP_River`. Agents used `SceneTools.add_to_scene_from_class` for ocean/lake. Catalog `use_when` still lists lake/ocean. |

### Proposed API

**Tool:** `UeremcpEnvironment.UeremcpEnvironmentToolset.CreateWaterBody` (extend) **or** new `CreateWaterBodies`.

```json
{
  "protocol_version": "1.0",
  "action": "create_water_body",
  "request_id": "northridge-water-1",
  "options": {
    "dry_run": false,
    "save": true,
    "validate": true,
    "replace_owned": "label"
  },
  "specification": {
    "seed": 73131,
    "bodies": [
      {
        "id": "main_river",
        "body_type": "river",
        "label": "UEREMCP_River_Main",
        "river": { "width": 900 }
      },
      {
        "id": "coast_ocean",
        "body_type": "ocean",
        "label": "UEREMCP_Ocean_Coast",
        "ocean": {
          "center": [0, -16000, -120],
          "extents_cm": [60000, 30000],
          "water_material": "/Water/Materials/Water_Material_Ocean"
        }
      },
      {
        "id": "oakendale_lake",
        "body_type": "lake",
        "label": "UEREMCP_Lake_Oakendale",
        "lake": {
          "center": [8500, 3200, -80],
          "radius_cm": 2500
        }
      }
    ]
  }
}
```

**Response (sketch):**

```json
{
  "status": "created",
  "result": {
    "created": [
      {"id": "main_river", "label": "UEREMCP_River_Main", "class": "WaterBodyRiver"},
      {"id": "coast_ocean", "label": "UEREMCP_Ocean_Coast", "class": "WaterBodyOcean"},
      {"id": "oakendale_lake", "label": "UEREMCP_Lake_Oakendale", "class": "WaterBodyLake"}
    ],
    "replaced": [],
    "unsupported": []
  }
}
```

**Rules:**
- Unknown/Phase-2 `body_type` → `rejected` or `partially_completed` with `unsupported[]` — **never** silently spawn river.
- Default `replace_owned=label`: only replace actors matching that body's label.
- `replace_owned=all_water` requires `options.confirm_wipe=true`.
- Single-body legacy envelope (`specification.river` only) remains; still must not wipe unrelated water labels.

### Acceptance tests

- **Given** landscape exists; **When** create river then ocean then lake; **Then** three distinct WaterBody actors remain; river spline length unchanged.
- **Given** `body_type=ocean` before implementation complete; **When** call with `on_unsupported=fail`; **Then** `rejected` + hint to SceneTools **or** `unsupported` — not a river.
- **Given** existing `UEREMCP_River_Main`; **When** create lake only; **Then** river actor still present.
- Catalog CI: `use_when` must not list lake/ocean until implemented.

### Notes

Compat: keep `action=create_water_body`. Migrate catalog + schema Phase 2 markers. Prefer implementing ocean/lake over documenting SceneTools forever.

---

## MCP-002 — SnapActorsToLandscape (ignore foliage)

| | |
|--|--|
| **Priority** | P0 |
| **Title** | SnapActorsToLandscape |
| **Problem** | Castle/huts/boardwalk floated; origin line traces hit tree tops. Manual `SetActorTransform` loops (~30 mentions). No landscape-only snap helper. |

### Proposed API

```json
{
  "protocol_version": "1.0",
  "action": "snap_actors_to_landscape",
  "specification": {
    "actors": ["NorthridgeCastle", "Stonehaven_*", "Seabreak_*", "Oakendale_*"],
    "actor_labels": ["PlayerStart"],
    "trace": {
      "channel": "Visibility",
      "accept_classes": ["Landscape", "LandscapeStreamingProxy", "LandscapeMeshProxyActor"],
      "ignore_classes": ["InstancedFoliageActor", "HierarchicalInstancedStaticMeshActor", "WaterBody*"],
      "ignore_actor_labels_prefix": ["UEREMCP_Forest", "UEREMCP_Foliage"]
    },
    "offset_z_cm": 0,
    "align_to_slope": false,
    "max_snap_error_cm": 50
  }
}
```

**Response:**

```json
{
  "status": "created",
  "result": {
    "snapped": [
      {"label": "NorthridgeCastle", "from_z": 1200, "to_z": 727, "hit": "Landscape"}
    ],
    "failed": [
      {"label": "Road_07", "reason": "no_landscape_hit", "hint": "move XY over landscape"}
    ]
  }
}
```

### Acceptance tests

- **Given** cube above foliage canopy over landscape; **When** snap with defaults; **Then** Z equals landscape height ±5 cm, not foliage top.
- **Given** actor off landscape; **When** snap; **Then** entry in `failed` with reason, not silent no-op success.
- Automation: place known Z mismatch fixture → snap → assert Z.

### Notes

Can wrap programmatic traces; expose as Environment or Scene goal tool. Document that `align_to_slope=true` rotates actor.

---

## MCP-003 — ClearFoliageInVolumes / path corridor clearings

| | |
|--|--|
| **Priority** | P0 |
| **Title** | ClearFoliageInVolumes |
| **Problem** | Trees clipped castle; roads blocked. `WaterBodyExclusionVolume` does **not** cull HISMC/PCG foliage. No instance cull API. |

### Proposed API

```json
{
  "protocol_version": "1.0",
  "action": "clear_foliage_in_volumes",
  "specification": {
    "volumes": [
      {"type": "box", "center": [200, 800, 700], "extent": [4000, 4000, 2000], "label": "CastleClear"},
      {"type": "spline_corridor", "spline_actor": "Northridge_RoadSpline", "half_width_cm": 400, "height_cm": 2000}
    ],
    "targets": {
      "foliage_actor_label_prefix": "UEREMCP_",
      "groups": ["*", "forest", "grass_v3"],
      "remove_mode": "instances"
    },
    "also_block_future_scatter": true
  }
}
```

**Response:**

```json
{
  "status": "created",
  "result": {
    "removed_instances": 1823,
    "volumes_registered_for_scatter_exclude": ["CastleClear", "corridor:Northridge_RoadSpline"]
  }
}
```

### Acceptance tests

- **Given** dense scatter overlapping castle box; **When** clear; **Then** instance count in box = 0; outside box unchanged (±1%).
- **Given** `also_block_future_scatter=true`; **When** ScatterFoliage again; **Then** `exclusion_violations` for cleared volumes = 0.
- Negative: empty volumes → `rejected`.

### Notes

Must mutate HISMC instance arrays / foliage IFAs, not only place water exclusion volumes. Integrate exclusion masks into `ScatterFoliage`.

---

## MCP-004 — Landscape height+slope layer paint / auto-paint

| | |
|--|--|
| **Priority** | P0 |
| **Title** | PaintLandscapeLayers / auto-paint after material assign |
| **Problem** | `CreateLandscapeMaterial` assigned layers but ground stayed white/flat; no weight-map paint API. Agents invented world-Z auto materials as stopgap. |

### Proposed API

```json
{
  "protocol_version": "1.0",
  "action": "paint_landscape_layers",
  "target": { "landscape_label": "UEREMCP_Landscape" },
  "specification": {
    "mode": "auto_height_slope",
    "material_path": "/Game/__UeremcpPoc/Northridge/M_NorthridgeTerrain",
    "layers": [
      {"name": "sand", "max_height_m": 80, "max_slope_deg": 25},
      {"name": "grass", "min_height_m": 60, "max_height_m": 950, "max_slope_deg": 32},
      {"name": "rock", "min_slope_deg": 28},
      {"name": "snow", "min_height_m": 1350}
    ],
    "normalize_weights": true
  }
}
```

**Or** extend `create_landscape_material` with `"options":{"auto_paint":true,"assign":true}`.

**Response:** `painted_layers[]`, `coverage_pct` per layer, `mean_albedo_luma` sample (optional).

### Acceptance tests

- **Given** layered LM assigned, weights zero; **When** auto-paint; **Then** each named layer has coverage_pct > 0 on mountains profile; capture luma not near 1.0 uniform white.
- **Given** unknown layer name; **When** paint; **Then** `failed_validation` listing valid layer info object names.
- Golden: heightmap fixture → expected coverage bands within ±10%.

### Notes

AlphaBrush sculpt remains out of scope; this is weight painting / LandscapeEdit layers only.

---

## MCP-005 — Batch primitive/actor ops & first-class ExecutePlan guidance

| | |
|--|--|
| **Priority** | P0 |
| **Title** | BatchActorOps + ExecutePlan-first surfacing |
| **Problem** | Cities/roads = many round-trips. `ExecutePlan` ~10 mentions vs ~331 call_tool; greybox agent: "discovered late." `execute_tool_script` worked but requires `get_execution_environment` first. |

### Proposed API

**A. Guidance (no new tool required for MVP):**  
`GetStarted` / `ResolveIntent` for environment intents must return:

```json
{
  "recommended_batching": {
    "prefer": ["ExecutePlan", "execute_tool_script"],
    "example_plan_ref": "Content/IntentRouter/examples/northridge_execute_plan.json",
    "avoid": "per-primitive CallMcpTool loops for >5 actors"
  }
}
```

**B. Tool:** `BatchActorOps`

```json
{
  "protocol_version": "1.0",
  "action": "batch_actor_ops",
  "specification": {
    "ops": [
      {"op": "spawn", "class": "/Script/Engine.StaticMeshActor", "label": "Hut_01", "location": [1,2,3], "mesh": "/Game/.../SM_Hut"},
      {"op": "set_transform", "label": "Hut_01", "location": [1,2,10], "rotation": [0,0,45]},
      {"op": "set_material", "label": "Hut_01", "slot": 0, "material": "/Game/.../M_Wood"}
    ],
    "atomic": false
  }
}
```

**C. ExecutePlan example (Environment):**

```json
{
  "protocol_version": "1.0",
  "action": "execute_plan",
  "specification": {
    "steps": [
      {"tool": "CreateLandscape", "request": {"$ref": "step0"}},
      {"tool": "CreateWaterBody", "request": {"$ref": "step1"}},
      {"tool": "ScatterFoliage", "request": {"$ref": "step2"}}
    ]
  }
}
```

(Exact ExecutePlan schema should match live `UeremcpCore` tool; ship a worked region example in catalog.)

### Acceptance tests

- GetStarted environment detail includes `recommended_batching.prefer` containing `ExecutePlan`.
- BatchActorOps 20 spawns → single MCP round-trip; all labels present.
- Catalog example_request for BuildEnvironment references ExecutePlan composition (COVERAGE_PLAN III.8).

### Notes

Do not remove programmatic script; promote it in GetStarted. Agent skills / Cursor rules can mirror this.

---

## MCP-006 — ScatterFoliage must not mutate heightmap

| | |
|--|--|
| **Priority** | P0 |
| **Title** | ScatterFoliage heightmap immutability (or opt-in) |
| **Problem** | HF upgrade: "ScatterFoliage appears to mutate landscape heightmap (hash changes)"; agents re-snapped after every scatter. Side effect undocumented. |

### Proposed API

```json
{
  "action": "scatter_foliage",
  "options": {
    "mutate_landscape": false
  },
  "specification": {
    "seed": 42,
    "biome": { "mesh_path": "/Game/.../PCG_Tree_01", "group": "forest" }
  }
}
```

**Rules:**
- Default `mutate_landscape=false`. If implementation currently edits heights (water carve / bank sculpt), move that behind `mutate_landscape=true` or separate `CarveLandscapeForHydrology`.
- Response always includes `landscape_heightmap_hash_before` and `landscape_heightmap_hash_after`.
- If hashes differ while `mutate_landscape=false` → `failed_validation`.

### Acceptance tests

- **Given** landscape hash H; **When** scatter with default options; **Then** hash == H; foliage_instances > 0.
- **Given** `mutate_landscape=true` (if supported); **When** scatter; **Then** hash may change; `change_manifest.landscape_mutated=true`.

### Notes

If mutation comes from shared `Build()` wipe/reimport, fix MCP-015 first — may eliminate false hash changes.

---

## MCP-007 — DescribeOperation / toolset slim mode + caching

| | |
|--|--|
| **Priority** | P0 |
| **Title** | Slim describe payloads + cache |
| **Problem** | ~61 describe_toolset mentions; Environment dump ~83 KB with duplicated requestJson schemas; agents Shell-grepped names. |

### Proposed API

```json
{
  "toolset_name": "UeremcpEnvironment.UeremcpEnvironmentToolset",
  "detail": "index"
}
```

**`detail` enum:** `index` | `tools` | `full` (default `index` for MCP describe_toolset wrapper if controllable; else add `DescribeToolset`).

**index response:**

```json
{
  "toolset": "UeremcpEnvironment.UeremcpEnvironmentToolset",
  "etag": "env-0.3.0-abc",
  "tools": [
    {
      "name": "CreateLandscape",
      "action": "create_landscape",
      "one_liner": "Seeded heightmap landscape",
      "destructive": true,
      "describe_operation": "UeremcpEnvironment...CreateLandscape"
    }
  ]
}
```

**DescribeOperation** add:

```json
{
  "specification": {
    "tool": "UeremcpEnvironment.UeremcpEnvironmentToolset.CreateLandscape",
    "detail": "slim",
    "include_example": true,
    "include_full_schema": false
  }
}
```

Slim: description, required fields, example_request, gotchas[], not mirrored contentSchema.

### Acceptance tests

- index payload < 8 KB for Environment toolset.
- slim DescribeOperation < 4 KB and includes example_request.
- If client sends `If-None-Match: etag` and unchanged → `status=no_change_required` empty body.

### Notes

Cursor MCP host may still cache poorly; shrink anyway. Prefer ResolveIntent → DescribeOperation over describe_toolset full.

---

## MCP-008 — Import mesh with unit scale + collision presets (one-shot)

| | |
|--|--|
| **Priority** | P1 |
| **Title** | ImportMeshForWorld |
| **Problem** | Blender FBX → import → cm/scale fights → collision preset separate. Grass underscaled; castle needed scale 100 + `CTF_UseComplexAsSimple`. |

### Proposed API

```json
{
  "protocol_version": "1.0",
  "action": "import_mesh_for_world",
  "specification": {
    "source_path": "C:/.../SM_NorthridgeCastle.fbx",
    "destination_path": "/Game/__UeremcpPoc/Northridge/Meshes/SM_NorthridgeCastle",
    "unit": "cm",
    "source_unit": "m",
    "uniform_scale": 100,
    "nanite": true,
    "collision": "complex_as_simple",
    "generate_lightmap_uvs": true,
    "material_slot_overrides": [
      {"slot": "Stone", "material": "/Game/.../M_CastleStone_PBR"}
    ]
  }
}
```

**Response:** `asset_path`, `bounds_cm`, `collision_preset_applied`, `nanite_enabled`, `import_scale_used`.

### Acceptance tests

- Import 1 m Blender unit cube with `source_unit=m`, `unit=cm` → bounds ~100 cm.
- `collision=complex_as_simple` → mesh body setup enum matches.
- Missing file → `rejected` with path hint.

### Notes

Wraps StaticMeshTools.import_file + property sets. Cross-link Blender export helper recommending cm.

---

## MCP-009 — Weather / rain one-shot

| | |
|--|--|
| **Priority** | P1 |
| **Title** | AttachWeather defaults + region plan hook |
| **Problem** | `AttachWeather` exists but was barely used (~2 mentions) until user demanded rain. Discovery lag. |

### Proposed API

Keep tool; tighten defaults + GetStarted:

```json
{
  "protocol_version": "1.0",
  "action": "attach_weather",
  "specification": {
    "seed": 73131,
    "weather": {
      "kind": "rain",
      "follow": "player_camera",
      "intensity": 0.7,
      "rain_system_path": null
    },
    "fallback_policy": "allow_approximate"
  }
}
```

`plan_environment` / BuildEnvironment `include.weather=true` should emit filled AttachWeather step when user intent mentions rain/storm.

### Acceptance tests

- Intent "make it rain" → ResolveIntent recommends AttachWeather with example_request.
- AttachWeather with missing Niagara + allow_approximate → `created_with_warnings` + visible proxy OR explicit unsupported — not silent no-op.
- PIE follow distance metric present when follow=player_camera.

### Notes

Low implementation cost; mostly catalog/GetStarted surfacing.

---

## MCP-010 — Grounded place prefab (castle/city) with foundation clear

| | |
|--|--|
| **Priority** | P0 |
| **Title** | PlacePrefabOnLandscape |
| **Problem** | Castle required Blender + import + scale + snap + foliage clear + still floated on canopy. Cities similar. |

### Proposed API

```json
{
  "protocol_version": "1.0",
  "action": "place_prefab_on_landscape",
  "specification": {
    "asset_path": "/Game/__UeremcpPoc/Northridge/Meshes/SM_NorthridgeCastle",
    "label": "NorthridgeCastle",
    "location_xy": [200, 800],
    "yaw_deg": 0,
    "uniform_scale": 100,
    "snap": { "ignore_foliage": true, "offset_z_cm": 0 },
    "foundation": {
      "clear_foliage_padding_cm": 1500,
      "flatten_radius_cm": 0,
      "embed_cm": 50
    },
    "collision": "complex_as_simple"
  }
}
```

**Internals:** import optional if `source_fbx` provided; MCP-002 snap; MCP-003 clear; optional minor landscape flatten (explicit opt-in).

### Acceptance tests

- Place prefab → actor Z on landscape; no foliage instances in padding box; collision complex-as-simple.
- Without landscape → `rejected`.
- One MCP call replaces ≥5 prior calls (import/transform/snap/clear).

### Notes

Highest UX win for "hero structure" workflows. PlaceStructures kinds stay for barrier demos.

---

## MCP-011 — Better error contracts

| | |
|--|--|
| **Priority** | P0 |
| **Title** | Structured errors with next_args |
| **Problem** | Scale rejection, dirty map, missing mesh, VFX-only material — agents retried blindly (4× BuildEnvironment early). |

### Proposed API (response envelope addition)

```json
{
  "protocol_version": "1.0",
  "status": "rejected",
  "error": {
    "code": "NONUNIFORM_SCALE_FORBIDDEN",
    "message": "terrain.scale_xy (100) != terrain.scale_z (3.2)",
    "hint": "For mountains use scale_xy=100, scale_z=3 with allow_nonuniform_scale=true; or set both equal. Raise max_altitude_m for taller peaks — do not raise scale_z past ~5.",
    "next_args": {
      "specification.terrain.scale_xy": 100,
      "specification.terrain.scale_z": 3,
      "specification.terrain.allow_nonuniform_scale": true
    },
    "docs_ref": "UeremcpEnvironment.CreateLandscape#scale_z"
  }
}
```

**Required codes (min set):**  
`NONUNIFORM_SCALE_FORBIDDEN`, `DIRTY_MAP_BLOCKS_SWITCH`, `MESH_PATH_NOT_FOUND`, `BODY_TYPE_UNSUPPORTED`, `TOOL_VFX_ONLY`, `REPLACE_OWNED_WIPE_CONFIRM_REQUIRED`, `SEMANTIC_SEARCH_UNAUTHORIZED`.

### Acceptance tests

- Nonuniform without flag → code + next_args recipe above.
- Dirty untitled destination → `DIRTY_MAP_BLOCKS_SWITCH` + recovery save_as path.
- CreateMasterMaterial for opaque surface → `TOOL_VFX_ONLY` + next tool `CreateLandscapeMaterial` or MaterialTools.

### Notes

Align with ADR statuses (`rejected`, `failed_validation`). Machine-readable `next_args` is the point.

---

## MCP-012 — Foliage species discovery / grass pack helpers

| | |
|--|--|
| **Priority** | P1 |
| **Title** | ListFoliageCandidates / EnsureGrassPack |
| **Problem** | Catalog invented `/Game/Meshes/SM_Pine`; Nanite grass missing; agents eventually found PCG trees; Blender grass looked mint/flat. |

### Proposed API

```json
{
  "protocol_version": "1.0",
  "action": "list_foliage_candidates",
  "specification": {
    "roles": ["tree_evergreen", "tree_deciduous", "grass", "shrub"],
    "query": "*Tree*",
    "prefer_nanite": true,
    "limit": 20
  }
}
```

**Response:**

```json
{
  "candidates": [
    {"role": "tree_evergreen", "path": "/Game/PCG/…/PCG_Tree_01", "nanite": false, "score": 0.82}
  ],
  "missing_roles": ["grass"],
  "hints": ["Import grass cards or enable Megascans pack; Blender clump fallback documented"]
}
```

Optional `ensure_starter_foliage_pack` copies plugin Content sample trees/grass into `/Game/__UeremcpPoc/Foliage/`.

### Acceptance tests

- Never return non-existent paths.
- Fantasy forest intent plan uses only AssetRegistry-resolved mesh_path.
- missing_roles includes grass when none present.

### Notes

Replaces SemanticSearch dependency for foliage (SemanticSearch returned 401 in-session).

---

## MCP-013 — Watch / screenshot server reliability

| | |
|--|--|
| **Priority** | P1 |
| **Title** | Fix user-unreal-watch or deprecate |
| **Problem** | Live `serverStatus=error`; ~20 transcript mentions, 0 successful tools. Agents also juggled CaptureViewport vs CaptureWorldFrames. |

### Proposed API / product rules

1. Fix watch MCP process stability **or** remove from recommended GetMcpTools list with explicit `unavailable_reason`.
2. GetStarted visual guidance:

```json
{
  "screenshots": {
    "prefer": "EditorAppToolset.CaptureViewport",
    "structural_world": "UeremcpValidation.CaptureWorldFrames",
    "do_not_use": ["user-unreal-watch"]
  }
}
```

### Acceptance tests

- If watch server down, GetStarted lists it under `mcp_servers.unavailable`.
- CaptureViewport success path documented as primary beauty check; CaptureWorldFrames for structural evidence.

### Notes

Duplication between CaptureViewport and CaptureWorldFrames is OK if roles are explicit.

---

## MCP-014 — SemanticSearch auth / graceful degrade

| | |
|--|--|
| **Priority** | P2 |
| **Title** | SemanticSearch 401 contract |
| **Problem** | Search returned 401 missing API key (~7 mentions); wasted discovery turns. |

### Proposed API

On missing key:

```json
{
  "status": "rejected",
  "error": {
    "code": "SEMANTIC_SEARCH_UNAUTHORIZED",
    "hint": "Set SemanticSearch API key in project settings, or use AssetTools.find_assets / list_foliage_candidates",
    "next_tool": "editor_toolset.toolsets.asset.AssetTools.find_assets"
  }
}
```

### Acceptance tests

- No key → rejected with next_tool, not raw 401 HTML/JSON.
- GetStarted flags semantic_search: disabled.

---

## MCP-015 — Additive staging (stop full UEREMCP_* wipe)

| | |
|--|--|
| **Priority** | P0 |
| **Title** | replace_owned stage semantics |
| **Problem** | Research + field: stage tools call `Build()` → `DestroyOwnedEnvironmentActors` with empty prefix; landscape/river/foliage wipe each other. Same family as water overwrite. |

### Proposed API

```json
{
  "options": {
    "replace_owned": "stage",
    "confirm_wipe": false
  }
}
```

| replace_owned | Behavior |
|---------------|----------|
| `stage` (default for CreateLandscape / CreateWaterBody / ScatterFoliage) | Destroy only that stage's labels |
| `group` | Foliage: only `vegetation.group` actor |
| `none` | Additive; fail if label collision |
| `all` | Full wipe; requires `confirm_wipe=true` |

Response includes `change_manifest.replaced_owned_actors` + `preserved_owned_actors`.

### Acceptance tests

- CreateLandscape → CreateWaterBody → landscape actor still present.
- ScatterFoliage → landscape + river present.
- `replace_owned=all` without confirm → `rejected` / `REPLACE_OWNED_WIPE_CONFIRM_REQUIRED`.

### Notes

Code touchpoints documented in RB-Northridge backlog (`DestroyOwnedEnvironmentActors`, `DispatchEnvAction`). **Highest leverage fix in the plugin.**

---

## MCP-016 — plan_environment with filled request_json + asset truth

| | |
|--|--|
| **Priority** | P1 |
| **Title** | plan_environment |
| **Problem** | ResolveIntent suggested ghost meshes; agents burned turns. Need executable plan + missing/unsupported list. |

### Proposed API

```json
{
  "protocol_version": "1.0",
  "action": "plan_environment",
  "specification": {
    "intent": "Northridge: mountains, ocean, river, lake, 3 cities, castle, roads, evergreen, grass",
    "quality": "realistic",
    "seed": 73131,
    "destination_level_path": "/Game/__UeremcpPoc/Northridge/Northridge"
  }
}
```

**Response:** `steps[].request_json` filled, `asset_resolution.resolved|missing`, `unsupported_capabilities` (e.g. ocean until MCP-001), `round_trip_budget`.

### Acceptance tests

- Never emit `/Game/Meshes/SM_Pine` unless package exists.
- Ocean before MCP-001 → listed unsupported with degrade guidance.
- Northridge starter scenario ≤ 5 mutating calls after plan (see RB §D).

### Notes

Can be ResolveIntent mode=`plan_environment`. Pair with MCP-012.

---

## MCP-017 — Uniform / nonuniform scale metrics on CreateLandscape

| | |
|--|--|
| **Priority** | P0 |
| **Title** | Hard scale policy + structural_metrics |
| **Problem** | Needle terrain from bad scale_z; nonuniform fights; flatness invisible. Partially documented; still burned retries. |

### Proposed API

Reject nonuniform unless `allow_nonuniform_scale=true`. Always return:

```json
"structural_metrics": {
  "actor_scale": {"x": 100, "y": 100, "z": 3},
  "uniform_scale": false,
  "height_range_cm": 12345,
  "flat_area_pct": 18.2,
  "slope_p50_deg": 12.4,
  "slope_p95_deg": 41.0,
  "heightmap_hash": "…"
}
```

Pair with MCP-011 `NONUNIFORM_SCALE_FORBIDDEN` next_args.

### Acceptance tests

- `scale_xy=300,scale_z=100` → rejected.
- Mountains profile returns flat_area_pct; optional gate in ValidateEnvironment.

### Notes

Overlaps RB P0-3; keep as explicit fieldtest ID.

---

## MCP-018 — Capture preference / dedupe guidance

| | |
|--|--|
| **Priority** | P2 |
| **Title** | Single screenshot recommendation |
| **Problem** | CaptureViewport ~36 vs CaptureWorldFrames ~4; roles unclear. |

### Proposed API

Documentation + GetStarted only (see MCP-013). Optional alias tool `CaptureLevelView` → Viewport for agents.

### Acceptance tests

- GetStarted returns prefer CaptureViewport for beauty, CaptureWorldFrames for structural.

---

## Implementation order (suggested)

1. MCP-015 additive staging  
2. MCP-001 multi-water  
3. MCP-011 error contracts + MCP-017 scale  
4. MCP-002 snap + MCP-003 foliage clear + MCP-010 place prefab  
5. MCP-004 landscape auto-paint  
6. MCP-006 foliage height immutability  
7. MCP-007 slim describe + MCP-005 ExecutePlan surfacing  
8. MCP-008 import one-shot + MCP-012 foliage catalog  
9. MCP-009 weather surfacing + MCP-016 plan_environment  
10. MCP-013/014 watch + semantic search  

---

## Traceability — field failure → backlog

| Field failure | IDs |
|---------------|-----|
| CreateWaterBody overwrite / river-only | MCP-001, MCP-015 |
| Floating castle/huts; traces hit trees | MCP-002, MCP-010 |
| Trees in castle / roads; exclusion volumes useless | MCP-003 |
| White/flat landscape after material assign | MCP-004 |
| Many round-trips; ExecutePlan late | MCP-005, MCP-007, MCP-016 |
| ScatterFoliage heightmap hash change | MCP-006, MCP-015 |
| Huge describe_toolset dumps | MCP-007 |
| Blender unit/collision pain | MCP-008, MCP-010 |
| Rain requested late | MCP-009 |
| Scale reject loops | MCP-011, MCP-017 |
| No grass / invented pine paths | MCP-012, MCP-016 |
| unreal-watch dead | MCP-013 |
| SemanticSearch 401 | MCP-014 |
| Stage wipe landscape | MCP-015 |
