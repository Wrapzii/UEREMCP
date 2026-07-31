# RB-Northridge — MCP field cycle validation report

**Audience:** UEREMCP product owner  
**Date:** 2026-07-31  
**Branch:** `ws-11-northridge-remaining-impl`  
**Validation commit:** `56393f2394a90d604815cda0ee280bbf5e54e46b`  
**Message:** `[WS-11] Close Northridge field gaps with goal world APIs`  
**Field sources:** `MCP_Field_Report_Northridge.md`, `MCP_Backlog_API_Shapes.md`, `RB-Northridge-remaining-impl.md`, `RB-Northridge-fieldtest-mcp-backlog.md`, live MCP on `ueremcp_fieldtest`

---

## Fixes (P0 follow-up)

Follow-up on this branch closes the two validation P0 holes from §1 / §6:

| P0 | Fix | Evidence |
|----|-----|----------|
| `LAYER_NOT_FOUND` on paint | `paint_landscape_layers` auto-creates/assigns `ULandscapeLayerInfoObject` via `UE::Landscape::CreateTargetLayerInfo` + `CreateTargetLayerSettingsFor` / `UpdateLayerInfoMap`; optional `specification.material_path` assigns the blend | UE 5.8 `BuildPlugin` **PASS**; `test_northridge_remaining.py` **18 PASS** (new ensure + staging contracts) |
| Additive water/foliage blocked by revision | Env revision `no_change_required` only when revision matches **and** requested stage actors already exist (`RequestedStagesAlreadyPresent`) | Same static + build; live re-verify pending (editor stalled/modal after binary install relaunch) |

Also: `NONUNIFORM_SCALE` `next_args` now suggests a **uniform** safe scale (mergeable); `MESH_BOUNDS_MISMATCH` deletes the just-imported asset. `flatten_pad` still honestly unsupported. Multi-water / `body_type` still open.

**Live MCP:** Binaries from this build were installed into the fieldtest plugin junction and the editor was relaunched; `user-unreal-mcp` did not become responsive (upstream `:8000` busy/modal, proxy `:8001` healthy). Re-run appendix §8 steps 6–8 after the editor is interactive — do not invent PASS for paint/additive until live actors + weights are observed.

---

## 1. Executive summary

Northridge field agents could greybox a fantasy region but burned hundreds of calls on invented mesh paths, describe spam, floating prefabs, silent bad imports, white landscapes, and staging overwrite. Commit `56393f2` ships the remaining goal APIs from the WS-11 impl spec and **does improve the core discovery / import / place / describe / recovery gaps**. Live MCP on fieldtest confirmed those paths. **At `56393f2` validation time:** paint was blocked (`LAYER_NOT_FOUND`); additive water/foliage after landscape hit env revision idempotency (`already matches revision` → `rivers=0` / `forest=0`). Those two P0s are addressed in the follow-up commit documented under **Fixes** above (static+build green; live re-check pending). `flatten_pad` remains honestly unsupported.

### Scorecard

| Area | Field complaint | After `56393f2` | Live result (`56393f2`) | After P0 fix commit |
|------|-----------------|-----------------|-------------------------|---------------------|
| Asset discovery | Invented `/Game/Meshes/SM_Pine` | `find_project_assets` | **PASS** | unchanged |
| Import scale | Silent 1/100 FBX | `import_mesh_for_world` bounds gate | **PASS** (`MESH_BOUNDS_MISMATCH` + `next_args`) | also deletes bad import asset |
| Prefab placement | Floating / canopy traces | `place_prefab_on_landscape` | **PASS** (snap); `flatten_pad` → `FLATTEN_PAD_UNSUPPORTED` | unchanged |
| Landscape paint | White / unpainted terrain | `paint_landscape_layers` | **BLOCKED** (`LAYER_NOT_FOUND`) | LayerInfo auto-ensure shipped; **live pending** |
| Describe spam | ~61 dumps / ~83 KB Env | slim + `content_hash` / `if_none_match` | **PASS** | unchanged |
| Error recovery | Prose recipes only | `error.code` + `next_args` | **PASS** | `NONUNIFORM_SCALE` next_args now uniform |
| Additive staging | Stage wipe / overwrite | Prior MCP-015 + this cycle re-check | **Still broken** (idempotency after landscape) | stage-presence gate shipped; **live pending** |
| Multi-water | River-only / overwrite | Earlier honesty work; not closed here | **Still open** | still open |
| Build | — | UE 5.8 `BuildPlugin` | **PASS** | **PASS** (rebuild) |
| Static acceptance | — | `test_northridge_remaining.py` (15) | **PASS** | **18 PASS** |

---

## 2. Background

Northridge was a live stress test: compose a starter fantasy region (terrain, multi-water, foliage, settlements, hero castle, materials, weather) through MCP. Field report findings:

- Agents succeeded at greybox + partial high-fidelity upgrade, then fought staging wipe, river-only water, missing foliage cull / landscape paint, scale contracts, and discovery payload bloat.
- Call mix (~331 `call_tool`, ~61 `describe_toolset`, ~91 `execute_tool_script`, ~10 late `ExecutePlan`) was ~40% missing goal APIs, ~25% destructive staging, ~20% discoverability, ~15% late batching — not primarily “bad agent instructions.”
- User-visible failures: floating castle/huts, trees through walls, white landscape, undersized castle, fake ocean/lake meshes, invented pine paths.

Earlier on the same branch (before this commit): MCP-006 heightmap mismatch gate, MCP-015 additive stage prefixes, scale metrics, CreateLandscapeMaterial, SnapActorsToLandscape / ClearFoliageInVolumes / AttachWeather surfacing, catalog water honesty. **This commit closed the remaining impl-spec items** (AssetRegistry probe, import one-shot, place+snap composition, paint API, slim describe+ETag, `next_args`).

---

## 3. What shipped

**Commit / branch:** `56393f2` on `ws-11-northridge-remaining-impl` (not pushed at validation time).  
**Scope:** ~27 files, +2766 / −212 — Environment goal ops, IntentRouter describe cache, envelope `error.next_args`, dual catalogs, registry snapshot, static tests.

| MCP IDs | API / surface | What it does |
|---------|---------------|--------------|
| MCP-012 / MCP-016 | `find_project_assets` (`FindProjectAssets`) | AssetRegistry role probe; `resolved` / `unresolved`; refuses empty-as-success during registry load |
| MCP-008 | `import_mesh_for_world` (`ImportMeshForWorld`) | Compose `StaticMeshTools.import_file` + units/collision/Nanite; reject on `expected_bounds_m` mismatch |
| MCP-010 | `place_prefab_on_landscape` (`PlacePrefabOnLandscape`) | Spawn → `LandscapeZAt` snap → foliage clear; `flatten_pad` → `partially_completed` / `FLATTEN_PAD_UNSUPPORTED` |
| MCP-004 | `paint_landscape_layers` (`PaintLandscapeLayers`) | Height/slope weight paint on live landscape (no recomputed heightmap) |
| MCP-007 | `describe_operation` slim / index / full | `detail`, `content_hash`, `if_none_match`; dropped duplicated `contentSchema` mirror |
| MCP-011 | Envelope `error.code` + `error.next_args` | Patch-merge recovery for scale, heightmap, bounds, realism, missing mesh, layer not found |
| MCP-013 / 014 (honesty) | GetStarted `external_mcp_capabilities` | Marks `user-unreal-watch` and `SemanticSearch` `available:false` |

Implementation touchpoints: `UeremcpAssetProbe.cpp`, `UeremcpImportMesh.cpp`, `UeremcpPlacePrefab.cpp`, `UeremcpLandscapePaint.cpp`, `UeremcpIntentRouter.cpp`, `UeremcpEnvironmentService.cpp`, `schemas/envelope/response.schema.json`, `operation_catalog.json` (×2), `tests/world_doc/test_northridge_remaining.py`.

Compile fixes landed in the same commit path: UE 5.8 `FJsonObject` key type (`FStringType`); `PhysicsCore` + BodySetup includes for import collision.

---

## 4. Build & test evidence

### Build

| Check | Result |
|-------|--------|
| UE 5.8 `RunUAT BuildPlugin -TargetPlatforms=Win64 -Rocket` | **PASS** (222 actions, `Result: Succeeded`) |
| Fieldtest plugin retarget | Binaries installed; `ueremcp_fieldtest/Plugins/UEREMCP` junction pointed at this repo; editor relaunched for live MCP |

### Static

| Check | Result |
|-------|--------|
| `tests/world_doc/test_northridge_remaining.py` | **15 PASS** — catalog roles, plan registration, `IsLoadingAssets`, slim/ETag contract, place/import/paint source contracts, external capability honesty |

### Live MCP (`user-unreal-mcp` on fieldtest)

| Check | Result | Fail / note |
|-------|--------|-------------|
| `find_project_assets` | **PASS** | Resolved pine/grass/walls under `/Game/__UeremcpPoc/Northridge`; no invented `/Game/Meshes/SM_Pine` |
| `import_mesh_for_world` + `expected_bounds_m` | **PASS** | `MESH_BOUNDS_MISMATCH` with both numbers + `next_args` |
| Import without `expected_bounds_m` | **PASS** | Imported; reported `actual_bounds_m` |
| `place_prefab_on_landscape` | **PASS** | Snap via `LandscapeZAt`; `flatten_pad` → `FLATTEN_PAD_UNSUPPORTED` / `partially_completed` |
| `paint_landscape_layers` | **BLOCKED** | Honest `LAYER_NOT_FOUND`; material create works; LayerInfos not auto-assigned to landscape |
| `describe_operation` slim + ETag | **PASS** | Slim payload; matching `if_none_match` → `not_modified: true` |
| `next_args` surface | **PASS** | Observed: `NEEDLE_SCALE_Z`, `NONUNIFORM_SCALE`, `MESH_BOUNDS_MISMATCH`, `HEIGHTMAP_MISMATCH`, `LAYER_NOT_FOUND` |

### Prior P0 re-check (same live session)

| Check | Result |
|-------|--------|
| `HEIGHTMAP_MISMATCH` (wrong seed) | Works — rejected |
| `replace_owned` landscape scope | Landscape create scoped to `UEREMCP_Landscape` |
| Additive `create_water_body` / `scatter_foliage` after landscape (same seed) | **Still broken** — idempotency `already matches revision`; creates nothing (`rivers=0`, `forest=0`) |

**Not claimed PASS:** end-to-end painted Northridge terrain; multi-body ocean/lake; full foundation sculpt (`flatten_pad`); additive water+foliage composition after landscape.

---

## 5. Complaint → outcome matrix

| Field complaint | Before | After `56393f2` | Outcome |
|-----------------|--------|-----------------|---------|
| Blind mesh inventing | Catalog/examples → `/Game/Meshes/SM_Pine`; AssetTools late | One `find_project_assets` | **Fixed** |
| Describe spam / huge payloads | ~61 `describe_toolset`; Env ~83 KB with nested schema mirrors | Slim/index + ETag skip | **Fixed** (measure traffic in next field session) |
| Silent bad import scale | Import “succeeds” at wrong size | Bounds gate rejects with numbers | **Fixed** |
| Floating actors / canopy Z | Per-actor traces hit foliage | Place+snap landscape-only | **Fixed** for one-shot prefab path |
| Trees in foundation | No clear API / wrong exclusion volumes | Place clears foliage radius (composition) | **Improved** (dedicated corridor clear already existed from earlier WS-11) |
| White / unpainted landscape | Material authored, weights never painted | Paint API exists; fails without LayerInfos | **Partial** — honest fail, not painted |
| Scale / heightmap recovery loops | Prose-only recipes | Machine-readable `next_args` | **Fixed** for covered codes |
| Stage wipe landscape↔water↔foliage | Full `UEREMCP_*` destroy | Stage prefixes earlier; live additive still blocked by revision idempotency | **Still broken** for additive water/foliage |
| Ocean/lake via CreateWaterBody | `body_type` ignored; catalog lied | Catalog honesty earlier; SceneTools still workaround | **Still open** (not this commit’s delivery) |
| Watch / SemanticSearch dead | Advertised, 0 useful calls | GetStarted marks unavailable | **Honesty only** (external) |
| Foundation pad sculpt | N/A | Explicit unsupported | **Deferred** (honest) |

---

## 6. Remaining gaps (prioritized)

| Pri | Gap | Why it matters | Honest fail / evidence |
|-----|-----|----------------|------------------------|
| **P0** | ~~Env revision idempotency blocks additive water/foliage~~ | Agents still cannot compose region stages | **Fixed in code** (stage-presence gate). Live re-proof pending after editor interactive. |
| **P0** | ~~Paint LayerInfo auto-assign~~ | White landscape complaint | **Fixed in code** (`CreateTargetLayerInfo` ensure). Live re-proof pending. |
| **P0** | Multi-water without overwrite / honor `body_type` (MCP-001) | Still SceneTools class-spawn for ocean/lake | Open from backlog |
| **P1** | `flatten_pad` on `place_prefab_on_landscape` | Castle/settlement pads still manual | `FLATTEN_PAD_UNSUPPORTED` / `partially_completed` |
| **P1** | Confirm `ScatterFoliage` does not mutate heightmap (or opt-in + document) | Field HF upgrade noted hash drift | MCP-006 gate helps; silent mutation claim needs re-proof |
| **P2** | Measure describe traffic post-slim; ExecutePlan-first GetStarted | Discovery tax may still be high if agents ignore slim | Spec acceptance: index &lt; 4 KB for Environment |
| **P2** | External: fix or remove `user-unreal-watch`; SemanticSearch auth | Dead tools confuse agents | GetStarted now says unavailable |

---

## 7. Recommendations (next engineering steps)

1. **Live re-verify paint + additive staging** after fieldtest editor is interactive with the P0-fix binaries — gate merge confidence on rivers/forest actors + painted weights, not static contracts alone.
2. **Implement or permanently refuse ocean/lake** — either ship `AWaterBodyOcean` / `AWaterBodyLake` under Environment, or keep `on_unsupported=fail` and SceneTools as documented fallback only (catalog already should not advertise).
3. **Ship `flatten_pad`** via landscape heightmap edit (`FLandscapeEditDataInterface`) behind the existing partially_completed path — do not leave agents guessing.
4. **Re-run a short Northridge composition script** (landscape → water → foliage → find assets → place prefab → paint) as a regression harness.
5. **Do not invent PASS** for paint or additive water until live evidence shows actors + painted weights in-level.

---

## 8. Appendix — how to manually re-verify

**Prereqs:** UE 5.8; `ueremcp_fieldtest` with plugin built from `56393f2` (or newer on `ws-11-northridge-remaining-impl`); editor open; `user-unreal-mcp` ready.

1. **Build:**  
   `RunUAT BuildPlugin -Plugin=<…>/UEREMCP.uplugin -Package=<pkg> -TargetPlatforms=Win64 -Rocket` → expect Succeeded.
2. **Static:**  
   `python tests/world_doc/test_northridge_remaining.py` → 15 OK.
3. **Discovery:** `FindProjectAssets` with roles `foliage.tree`, `foliage.grass`, `structure.wall` → real `/Game/...` paths, no `SM_Pine` ghost.
4. **Import gate:** `ImportMeshForWorld` with wrong `expected_bounds_m` → `rejected` / `MESH_BOUNDS_MISMATCH` + `next_args`; omit bounds → success + `actual_bounds_m`.
5. **Place:** `PlacePrefabOnLandscape` over landscape with foliage → actor Z on terrain; with `flatten_pad` → `partially_completed` + `FLATTEN_PAD_UNSUPPORTED`.
6. **Paint:** `PaintLandscapeLayers` after `CreateLandscapeMaterial` (+ optional `material_path`) → expect LayerInfos auto-created and weights written (no `LAYER_NOT_FOUND` for named rules).
7. **Describe:** `DescribeOperation` `detail=slim` → payload + `content_hash`; repeat with `if_none_match` → empty/`not_modified`.
8. **Additive regression:** `CreateLandscape` → same-seed `CreateWaterBody` → same-seed `ScatterFoliage` → assert river and forest actors exist (must not return idempotent no-op when those stage actors are missing).
9. **Scale `next_args`:** nonuniform or needle `scale_z` → `NONUNIFORM_SCALE` / `NEEDLE_SCALE_Z` with mergeable `next_args` that still pass validation.

---

## Related docs

| Doc | Role |
|-----|------|
| `docs/research/RB-Northridge-remaining-impl.md` | Impl spec for the 7 items this commit targeted |
| `docs/research/RB-Northridge-fieldtest-mcp-backlog.md` | Original P0 staging / realism backlog |
| `docs/research/RB-Northridge-api-shapes.md` | Imported API shapes + status table |
| Fieldtest `docs/MCP_Field_Report_Northridge.md` | Session evidence / call patterns |
| Fieldtest `docs/MCP_Backlog_API_Shapes.md` | MCP-001…018 backlog |
| Fieldtest mirror of this report | `docs/MCP_Northridge_Validation_Report.md` |
