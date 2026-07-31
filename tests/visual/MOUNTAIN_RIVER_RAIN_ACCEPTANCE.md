# MountainRiverRain live acceptance (WS-16)

Date: 2026-07-30  
Branch: `ws-16-environment-coverage`  
Map: `/Game/__UeremcpPoc/MountainRiverRain/MountainRiverRain`  
Revision: `env:f49a66b5`  
Seed: `4471`  
Request id: `ws16-build-final-3`

## Call telemetry (acceptance path)

| # | Toolset.tool | Role | Result |
|---|---|---|---|
| 1 | `UeremcpIntent.UeremcpIntentToolset.ResolveIntent` | router | top-1 `BuildEnvironment` (extra noisy steps also returned) |
| 2 | `UeremcpEnvironment.UeremcpEnvironmentToolset.BuildEnvironment` | mutate | `created_with_warnings` |
| 3 | `UeremcpEnvironment.UeremcpEnvironmentToolset.ValidateEnvironment` | structural | `no_change_required` |
| 4 | `UeremcpValidation.UeremcpVisualCaptureToolset.CaptureWorldFrames` | screenshots | PNGs written; tool reported `png_ok:false` (over-strict) |
| 5 | `EditorToolset.EditorAppToolset.StartPIE` | PIE | started |
| 6 | `UeremcpEnvironment.UeremcpEnvironmentToolset.InspectEnvironment` | PIE metrics | `weather_followed_10m=true` (~91 m tracked) |
| 7 | `editor_toolset.toolsets.actor.ActorTools.set_actor_transform` | move pawn | true |
| 8 | `UeremcpEnvironment.UeremcpEnvironmentToolset.ValidateEnvironment` (`require_weather_follow_10m`) | PIE gate | `no_change_required` |
| 9 | `EditorToolset.EditorAppToolset.StopPIE` | cleanup | ok |

**Semantic mutate calls for world creation: 1** (`BuildEnvironment`).  
**Agent-facing environment round-trips for create+validate: 2**.

## Structural gates (PIE validate)

- non_flat: true (height range ≈ 8315 cm)
- continuous river: true
- both banks populated: 43 / 30
- open channel: true (`exclusion_violations=0`)
- rain present: true
- weather_followed_10m: true (`weather_follow_distance_cm≈9246`)
- validation_elapsed_ms: ~0.34

## Technology honesty

| Capability | Mode |
|---|---|
| Landscape heightmap import | real |
| WaterBodyRiver | real (`bAffectsLandscape=false`; valley from heightmap) |
| Seeded forest HISMC | real |
| Rainy lighting + viewpoint | real |
| Rain camera follow transform | real (`AUeremcpWeatherFollower`) |
| Rain particle visuals | approximated (streak fallback; Niagara path missing) |

## Screenshots

Copied from RE `Saved/UEREMCP/WorldCapture/`:

- `tests/visual/mountain_river_rain/world_frame_00.png`
- `tests/visual/mountain_river_rain/world_frame_01.png`

## Blockers / notes

1. WS-16 not yet in `docs/WORK_ALLOCATION.md` → `check_ownership.py --ws WS-16` fails until WS-01/02 accept proposal.
2. CaptureWorldFrames writes valid PNGs but reports `failed_validation` / `png_ok:false` (WS-11 ownership).
3. Router returns extra steps beyond BuildEnvironment (intent noise, not environment defect).
4. No Niagara rain asset in RE → streak fallback with honest `approximated` technology note.
