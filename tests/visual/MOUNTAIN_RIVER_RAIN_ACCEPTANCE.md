# MountainRiverRain live acceptance (WS-16)

Date: 2026-07-30  
Branch: `ws-16-rain-niagara-create`  
Map: `/Game/__UeremcpPoc/MountainRiverRain/MountainRiverRain`  
Rain asset: `/Game/__UeremcpPoc/MountainRiverRain/NS_EnvRain`  
Seed: `4471`

## Status behavior (before → after)

| Condition | Before (`1e9574f` / deploy `7745d3b`) | After |
|---|---|---|
| Rain requested, no `rain_system_path` | Silent HISMC streak fallback; `created_with_warnings`; `approximated` tech | Creates real `UNiagaraSystem` via `CreateNiagaraEffect(precipitation)`; bind to `AUeremcpWeatherFollower` |
| Niagara create/load fails, `fallback_policy=prefer_real` (default) | N/A (always fell back) | `failed_validation` / weather gate fail — **no** silent streak |
| Niagara create/load fails, `fallback_policy=allow_approximate` | N/A | Streak allowed; `approximated: true` + explicit warning |
| ValidateEnvironment | `has_rain` only (streak counted) | Also requires `rain_niagara_bound` unless `gates.allow_approximated_rain=true` |

## Call telemetry (acceptance path)

**Specification must set explicit `include.*` flags** (all default false). Full-scene
acceptance uses:

```json
"include": {"terrain": true, "river": true, "forest": true, "rain": true, "lighting": true},
"fallback_policy": "allow_approximate"
```

| # | Toolset.tool | Role | Result |
|---|---|---|---|
| 1 | `UeremcpEnvironment.BuildEnvironment` | mutate | expect `created_and_validated` or `created_with_warnings` (secondary only); **not** streak-only success |
| 2 | `UeremcpEnvironment.ValidateEnvironment` | structural | `no_change_required` with `rain_real_ok=true` |
| 3 | `UeremcpValidation.CaptureWorldFrames` / `CaptureEffectFrames` | screenshots | rain pixels when rendering available |
| 4 | PIE + move ≥10m + Validate (`require_weather_follow_10m`) | follow gate | `weather_followed_10m=true` |

## Technology honesty (target)

| Capability | Mode |
|---|---|
| Landscape heightmap import | real |
| WaterBodyRiver | real |
| Seeded forest HISMC | real |
| Rainy lighting + viewpoint | real |
| Rain camera follow transform | real (`AUeremcpWeatherFollower`) |
| Rain particle visuals | **real** (`NS_EnvRain` Niagara; RecycleParticlesInView + HangingParticulates) |

## Residual limits

1. CreateNiagaraEffect for precipitation typically returns `partially_completed` (POC B six-role validated status not applicable); Environment accepts loaded `UNiagaraSystem` as real rain.
2. Precipitation uses engine emitter templates (not a bespoke meteorological rain graph). Particles must spawn; trajectory aesthetics are secondary to "real Niagara asset".
3. CaptureWorldFrames PNG honesty mismatch remains WS-11-owned when present.
4. Live verification evidence for this revision is recorded after RE rebuild + MCP run on this branch.

## Prior run (superseded)

Previous acceptance on `ws-16-environment-coverage` @ `1e9574f` reported `created_with_warnings` because "rain Niagara path missing → streak fallback". That disposition is **invalid** under current requirements and is replaced by the table above.
