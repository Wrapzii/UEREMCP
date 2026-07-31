# MountainRiverRain live acceptance (WS-16)

Date: 2026-07-30  
Branch: `ws-16-environment-coverage`  
Map: `/Game/__UeremcpPoc/MountainRiverRain/MountainRiverRain`  
Revision: `env:f49a66b5`  
Seed: `4471`  
Request id: `ws16-build-final-3`

## Call telemetry (acceptance path)

**Specification must set explicit `include.*` flags** (all default false). Full-scene
acceptance uses:

```json
"include": {"terrain": true, "river": true, "forest": true, "rain": true, "lighting": true},
"fallback_policy": "allow_approximate"
```

| # | Toolset.tool | Role | Result |
|---|---|---|---|
| 1 | `UeremcpIntent.UeremcpIntentToolset.ResolveIntent` | router | top-1 `BuildEnvironment` (extra noisy steps also returned) |
| 2 | `UeremcpEnvironment.UeremcpEnvironmentToolset.BuildEnvironment` | mutate | `created_with_warnings` |
| 3 | `UeremcpEnvironment.UeremcpEnvironmentToolset.ValidateEnvironment` | structural | `no_change_required` |
| 4 | `UeremcpValidation.UeremcpVisualCaptureToolset.CaptureWorldFrames` | screenshots | WS-16 run wrote PNGs but reported `png_ok:false`; superseded on integration by live `png_ok:true` after bounded reread fix |
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
2. **Resolved on integration:** CaptureWorldFrames live rerun returned
   `no_change_required`, `png_ok:true`, `png_files_reread:true`, and
   `stage_teardown_complete:true`.
3. Router extra steps beyond BuildEnvironment: score-gate landed on tip (1.3d);
   live re-proof after Core rebuild; offline plan is already 1-step BuildEnvironment.
4. No Niagara rain asset in RE → streak fallback with honest `approximated` technology note.
