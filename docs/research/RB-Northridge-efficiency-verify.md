# RB-Northridge — Goal-API efficiency live verify

**Date:** 2026-07-31  
**Branch:** `ws-11-northridge-remaining-impl`  
**Commits under test:** `56393f2` (goal APIs) + `4364c47` (LayerInfo ensure + additive staging) + **`efd3675`** (NEEDLE next_args both axes, river.width heightmap rebind, LandscapeZAt fallback)  
**Project / MCP:** `ueremcp_fieldtest` · `user-unreal-mcp` (Ping + GetStarted OK)  
**Scratch world:** `/Temp/Untitled_1` (did not open/destroy Northridge map)  
**Assets:** `/Game/__UeremcpPoc/EfficiencyProbe/`  
**DLL note:** Live verify of `efd3675` required rebuild (`UnrealEditor-UeremcpEnvironment.dll` 2026-07-31 18:46); stale 16:53 DLL still returned NEEDLE `next_args` with only `scale_z`.

Wall times below are **rough end-to-end MCP RTT** (agent → editor → response), not internal Unreal clocks.  
**RTT** = `call_tool` / `describe_*` round-trips counted for that scenario’s *successful path* (failed probes noted separately).

---

## Scorecard

| Scenario | Old pattern (approx RTT) | New RTT | Improved? | Notes |
|----------|--------------------------|---------|-----------|-------|
| **A. Asset discovery** | 5–15+ invent `/Game/Meshes/SM_Pine`, AssetTools loops | **1** | **Yes** | `find_project_assets` resolved real pine/grass under `/Game/__UeremcpPoc/Northridge/...` |
| **B. Describe spam** | ~61× `describe_toolset`; Env dump ~83 KB | **1–2** per op (+ ETag skip) | **Yes** | index/slim + `if_none_match` → `not_modified`; full Env `describe_toolset` still **55 537 B** |
| **C. Import bounds gate** | 1 “ok” silent wrong scale + later rediscovery | **1 reject + 1 ok** | **Yes** | `MESH_BOUNDS_MISMATCH`; asset deleted; correct `[2,2,2]` one-shot OK |
| **D. Place on landscape** | find → spawn → wrong Z → re-trace foliage → adjust (~5–8) | **1** when landscape collidable | **Yes** | **`efd3675` retest:** scale≈3 landscape + `place_prefab_on_landscape` → `created_with_warnings`, **Z=167.3** snap (LandscapeZAt) |
| **E. Paint layers** | material OK, paint `LAYER_NOT_FOUND` loops | **2** (mat + paint) | **Yes** | Auto LayerInfo + weights; no `LAYER_NOT_FOUND` |
| **F. Additive env** | revision no-op → rivers=0 / forest=0 | **3 stages** (+ hash traps) | **Yes** | **`efd3675` retest:** `create_water_body` + `river.width=1200` after scale=3 landscape → **no `HEIGHTMAP_MISMATCH`**; note rebinds baked width |
| **G. next_args recovery** | prose recipes, multi-guess | **2** (`NONUNIFORM` + `NEEDLE`) | **Yes** | **`efd3675` retest:** NEEDLE `(100,100)` → `next_args` `{scale_xy:3, scale_z:3}` → 2nd call `created_with_warnings` (**2 RTT**) |

---

## Scenario detail

### Setup (not scored)

| Call | Result | ~s |
|------|--------|----|
| `list_toolsets` | OK | ~2 |
| `Ping` (Reference) | `no_change_required` | ~1 |
| `GetStarted` | prefer Env; asset_probe note; watch/SemanticSearch unavailable | ~1 |
| `InspectEnvironment` | Untitled_1, landscapes=1 (pre-existing non-UEREMCP) | ~1 |

### A. Asset discovery — **PASS** (1 RTT, ~2 s)

```json
{"action":"find_project_assets","specification":{"roles":["foliage.tree","foliage.grass"],"class_filter":["StaticMesh"],"path_scopes":["/Game","/Engine/BasicShapes"],"max_per_role":5}}
```

- Status: `no_change_required`
- Resolved: `SM_PineTree`, `SM_PineIcon`, `SM_GrassVisible`, `SM_GrassClump*` under Northridge paths
- Unresolved: `[]`
- Vs inventing `/Game/Meshes/SM_Pine`: **fixed**

### B. Describe spam — **PASS** (~4 describe RTTs + 1 baseline)

| Call | detail | Outcome | Payload (approx) | ~s |
|------|--------|---------|------------------|-----|
| `describe_operation` create_landscape | index | hash `c76085fd…` | ≪ toolset | ~1 |
| `describe_operation` paint_landscape_layers | slim | hash `10a80a96…` + request_json | ~2–4 KB class | ~1 |
| same + `if_none_match` | slim | `not_modified: true`, body = hash only | **~190 B** | ~1 |
| `describe_operation` find_project_assets | slim | OK | slim | ~1 |
| `describe_toolset` Environment (baseline) | full | schemas for all tools | **55 537 B** | ~3 |

Vs old full toolset spam: agents that use **slim + ETag** cut traffic by ~10–100× per re-describe. Agents that still call `describe_toolset` remain wasteful.

### C. Import bounds gate — **PASS** (2 RTT, ~6 s total)

1. Wrong `expected_bounds_m` `[240,145,90]` on Engine `BlenderCube.fbx`  
   - `rejected` / `MESH_BOUNDS_MISMATCH`  
   - actual `[2,2,2]`; **imported asset deleted**  
2. Correct `[2,2,2]` → `created_with_warnings` → `/Game/__UeremcpPoc/EfficiencyProbe/SM_CubeOk`

### D. Place on landscape — **PASS with caveat** (ideal 1 RTT; probe burned extras)

| Attempt | Landscape | Result |
|---------|-----------|--------|
| D1–D3 @ XY on scale_xy=z=3 landscape | bounds ~±189 cm | `partially_completed` — **no landscape beneath pivot** (Visibility `LandscapeZAt` miss) |
| SnapActorsToLandscape | same | snapped **0** of candidates |
| D5 recreate scale 100×100 (`allow_extreme_scale_z`) + D6 place | large | **`created_with_warnings` — Z=7654.6 snap OK**; foliage clear 0 |

**Ideal new path:** 1× `place_prefab_on_landscape` when a collidable landscape exists.  
**Waste still present:** following NEEDLE/NONUNIFORM recovery to **uniform scale=3** yields a landscape that this editor build often **cannot Visibility-trace**, so place “succeeds” as float-at-Z=0. Agents need either collision fix or guidance to keep XY scale large (`allow_nonuniform` / `max_altitude`) while keeping Z sane.

Old multi-step float path (~5–8 RTT) still loses to the **1-RTT happy path**.

### E. Paint — **PASS** (2 RTT, ~5 s) — closes `4364c47` live gap

1. `CreateLandscapeMaterial` → `/Game/__UeremcpPoc/EfficiencyProbe/M_TerrainEff` (grass/rock/snow)  
2. `paint_landscape_layers` + `material_path`  
   - Status: `created_with_warnings`  
   - **Auto-created LayerInfo:** snow, rock, grass under `/Game/__UeremcpPoc/LandscapeLayers`  
   - Coverage written (snow ~50%, rock ~50%, grass fallback)  
   - **No `LAYER_NOT_FOUND`**

### F. Additive env — **PASS** (live proof of `4364c47`; hash traps cost RTTs)

Same seed `8801`, staged after landscape:

| Call | Result |
|------|--------|
| `create_water_body` without matching terrain | `NEEDLE_SCALE_Z` (defaults) |
| + terrain scales but `river.width:1200` | **`HEIGHTMAP_MISMATCH`** (width carves heightmap; hash ≠ landscape) |
| terrain-only match (default river width) | **`created_with_warnings`** — note: *“Revision matched but requested stage actors are missing — proceeding…”* |
| `scatter_foliage` + real pine mesh | created `UEREMCP_Forest` (status `failed_validation` on bank/open-channel gates; **actors exist**) |
| `InspectEnvironment` | **landscapes=2, rivers=1, forest=1, foliage_instances=1** — **not zeros** |

Vs old idempotent no-op: **fixed**. Remaining waste: agents must **byte-match** seed + terrain (+ default river width) or eat `HEIGHTMAP_MISMATCH` retries.

### G. next_args recovery — **PASS** (`efd3675` retest 2026-07-31 18:48)

| Path | Calls | Outcome |
|------|-------|---------|
| Provoke `scale_xy:100, scale_z:100` → NEEDLE | 1 | `next_args.terrain` = **`{scale_xy:3, scale_z:3}`** (both axes) |
| Merge next_args + `request_id` retry | 2 | `created_with_warnings` landscape actors=2 |
| (Prior) Provoke `scale_xy:100, scale_z:3` → NONUNIFORM | 1 | next_args `{scale_xy:3, scale_z:3}` |

**Claim “2 calls total” now holds for NEEDLE-alone** after rebuild of Environment DLL.

---

## Call-budget summary (this probe)

| Bucket | Approx `call_tool` / describe count |
|--------|-------------------------------------|
| Bootstrap (Ping/GetStarted/list/inspect) | ~5 |
| Scored happy-path equivalents (A–G ideal) | ~12–14 |
| Extra traps (place miss, HEIGHTMAP, NEEDLE chain, large-landscape redo) | ~10+ |
| **Session total (rough)** | **~35–40** MCP tool invocations |

Old Northridge field mix was hundreds of calls with ~40% missing goal APIs. Goal APIs **can** land the same intents in low-teens RTTs **if** agents follow slim describe, find_project_assets, matching terrain blocks, and collidable landscape scale.

---

## Still wasteful (honest)

1. ~~**`NEEDLE_SCALE_Z` next_args incomplete**~~ → **fixed in `efd3675`** (live 2-RTT).  
2. ~~**Uniform scale=3 landscape breaks place snap**~~ → **fixed in `efd3675`** (Z=167.3 on scale=3).  
3. ~~**`river.width` false HEIGHTMAP_MISMATCH**~~ → **fixed in `efd3675`** (width=1200 additive OK; note rebinds baked width).  
4. **`describe_toolset` still ~55 KB** if agents ignore slim/ETag.  
5. **Verbose `next_actions` on every Env response** still inflate tokens even when the op succeeded.  
6. **Scatter bank/open-channel gates** → `failed_validation` after successful stage create (confusing but not a no-op).  
7. ~~**Ocean/lake / flatten_pad**~~ → **fixed in follow-up** (`body_type` + SetHeightData pad; live re-proof pending).  
8. **Stale plugin binaries** — editor must load post-fix DLL or agents still see old NEEDLE / unsupported-pad behavior.  
9. ~~**Mutator FIFO hang**~~ → stale waiter/active clear + `MUTATOR_BUSY` retry guidance.  
10. ~~**GetStarted `do_not_use` watch**~~ → points at `check_unreal` / `get_editor_status`.

---

## Verdict

**Yes — the new goal APIs are efficient enough for agents on the core Northridge pain paths**, with live proof that `4364c47` paint LayerInfo ensure and additive water/foliage staging work (rivers>0, forest>0; paint without `LAYER_NOT_FOUND`). Discovery, import bounds, slim/ETag describe, and NONUNIFORM `next_args` each cut multi-call failure loops to 1–2 RTTs.

**`efd3675` closes the three prior efficiency blockers** (NEEDLE 2-RTT, river.width additive, place snap on scale=3) when the rebuilt Environment DLL is loaded. Remaining waste is mostly describe verbosity / scatter validation messaging, not recovery loops.

---

## Related

- `docs/research/RB-Northridge-validation-report.md`  
- Fieldtest mirror: `docs/MCP_Northridge_Efficiency_Verify.md`
