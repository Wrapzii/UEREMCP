# WS-01 backlog completion — 2026-07-30

**Branch:** `ws-01-backlog-integration`  
**Deploy tip (local):** `a4e225c` (junction from RE → `UEREMCP-deploy-main`)  
**Dirty Opus root:** not modified (read-only provenance for `COVERAGE_PLAN.md`)

## Deployment / SHA

| Ref | SHA | Notes |
|---|---|---|
| Baseline main/deploy | `82337de` | Pre-integration |
| Integration land | `434bc12` | Environment v0.1, echo, CaptureWorldFrames, ledger |
| Hang fix | `d9f4936` | WaterBrushManager MCP deadlock (`bAffectsLandscape=false`) |
| WS-16 harden adopt | `9edd138` | acceptance gates, Systems module, capture reread |
| Foliage gate fix | `a4e225c` | corridor-biased banks + 55° slope default |

RE junction: `...\RE\Plugins\UEREMCP` → `UEREMCP-deploy-main\Plugins\UEREMCP`.

## BACKLOG item ledger

See `docs/BACKLOG.md` Part "Backlog completion ledger".

## COVERAGE_PLAN delta (Part II–III)

Copied latest from dirty root (Parts II–III + III.1–III.11). Audited:

1. **GeometryScript "blocked"** — incorrect after enabling plugin. `AppendBox` present
   `[VERIFIED: MeshPrimitiveFunctions.h:168]`. `PlaceStructures` implemented.
2. **Module `UeremcpWorld`** — superseded by `UeremcpEnvironment` (WS-01 ownership).
3. **`attach_weather`** — added (gap called out in III.10).
4. **Plan composition** — `FUeremcpEnvironmentPlanHandlers` registers all stage
   actions with `FUeremcpPlanExecutor` (no second batching layer).
5. **`heightmap_hash`** — CRC of generated heights (III.4 / III.11.2).
6. **Exclusion re-measure** — post-scatter distance check (III.11.3).
7. **World capture** — `CaptureWorldFrames` (III.10 / BACKLOG 3.2).
8. **PCG** — audited; not duplicated for riverbank exclusion (W-DUP avoidance).
9. **Audio / networking / world partition** — `UeremcpSystems` goal tools landed
   (see Part IV + `ws-01-systems-live-handoff.md`); thin Epic surface still limits depth.

Full capability table: `docs/COVERAGE_PLAN.md` Part IV.

## Live verification (2026-07-30 evening)

`[VERIFIED-RUNTIME]` single-editor MCP after `a4e225c` rebuild:

| Call | Result |
|---|---|
| `GetStarted` | `prefer_toolsets` includes `UeremcpEnvironment…` |
| `ResolveIntent` (earlier session) | step1=`BuildEnvironment` confidence=high |
| `BuildEnvironment` dry_run seed=42 | `no_change_required`, `heightmap_hash=35f8b900` |
| `BuildEnvironment` mutate `mrr-build-004` | **`created_with_warnings`**, foliage=33, banks=6/27, exclusion=0, saved+reloaded, `heightmap_hash=9dde558d`, revision `env:19dc1524` |
| `ValidateEnvironment` | gates passed (landscape/river/forest/rain/open_channel/both_banks) |
| `CaptureWorldFrames` `mrr-cap-1` | 2/2 PNG reread OK @ 1280x720 → `Saved/UEREMCP/WorldCapture/MountainRiverRain/mrr-cap-1/` |

Call count for mutate path after map load: **Build → Validate → Capture = 3** (within 2–3 target when ResolveIntent omitted).

### Hang / foliage defects fixed live

1. Sync `SpawnActor<AWaterBodyRiver>` under MCP deadlocked at `WaterBrushManager::SetupDefaultMaterials`
   — deferred pre-spawn `bAffectsLandscape=false` `[VERIFIED: WaterBodyComponent.h:630]` /
   `[VERIFIED: WaterEditorModule.cpp:190]`.
2. Random-XY foliage + 32° slope rejected all bank candidates (`slope_rejected=168`, foliage=0)
   — corridor-biased `SampleAlongXY` + default slope 55° → both banks populated.

## Remaining external limitations

- PIE rain camera-follow distance gate still false without a project Niagara rain asset + movement samples (`weather_followed_10m=false`).
- Foliage/rain are **approximated** (cube HISMC / streak fallback) until `biome.mesh_path` / `weather.rain_system_path` supplied.
- Full `MOUNTAIN_RIVER_RAIN_ACCEPTANCE.md` human PNG checklist (player_start / rain A–B) is companion evidence — structural gates passed; screenshot aesthetic gate is human review.
- Focus mode still disabled until intentional enable after discoverability reconfirm.
