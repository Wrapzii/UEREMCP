# UEREMCP Environment toolset — API evidence & limits

**Owner:** WS-01 (unassigned domain → lead). **Date:** 2026-07-30.

## Audit before build

| Gap | Existing Epic/RE surface | Why insufficient |
|---|---|---|
| Landscape / terrain | 0 MCP tools (COVERAGE_PLAN) | No agent-facing heightmap import |
| Water | Water plugin enabled; 0 MCP tools | No river creation tool |
| Foliage | PCG 31 tools; dress_workflow scatter | PCG graphs are not a one-shot seeded riverbank forest with exclusion corridor |
| Procedural mesh | GeometryScripting enabled; 0 MCP tools | Not required for MountainRiverRain acceptance (landscape path preferred) |

## Verified APIs used

- `ALandscape::Import(...)` `[VERIFIED: LandscapeProxy.h:1418-1420]`
- `AWaterBodyRiver` `[VERIFIED: WaterBodyRiverActor.h:28]`
- `AWaterBody::GetWaterSpline()` `[VERIFIED: WaterBodyActor.h:103]`
- Seeded noise + spline corridor are pure UEREMCP helpers (BACKLOG 5.3/5.4/5.6)

## Batching

Internal sequence terrain → river → foliage → weather → capture. Uses existing envelope/job patterns; **does not** add a second batching layer (BACKLOG 5.7). Agents should call `BuildEnvironment` once (optionally `GetJobResult`).

## Limitations

- World "looks good" is **not** pixel-gated; structural metrics + human review (BACKLOG 5.8).
- Rain camera-follow is best-effort Niagara spawn; full PIE attach requires a rain system asset path.
- Foliage without `biome.mesh_path` uses `/Engine/BasicShapes/Cube` and reports `approximated`.
- Audio / networking / world-partition are **not** implemented inside BuildEnvironment yet — see closeout ledger.
