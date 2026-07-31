# WS-16 proposal — Environment v2 Phase 2 (not in this deliverable)

**Status:** proposed. **Owner:** WS-16 / WS-01 (environment schema).

Phase 1 (landed on `ws-16-environment-coverage`) delivers composable v2 schema parsing,
terrain profiles, multi-weather (snow+hail via CreateNiagaraEffect), and `ice_wall_ring`
structures. The following remain explicitly **not** implemented:

| Item | Blocker / note |
|---|---|
| `hydrology.lake` / `hydrology.ocean` | WaterBodyLake/Ocean spline API spike; schema stubs only |
| `vegetation` PCG masks | Audit PCG scatter before duplicating (COVERAGE_PLAN III) |
| `terrain.heightmap_recipe` import | Needs heightmap file ingest path |
| Winter foliage material swap | WS-08 material modifier over existing scatter |
| `viewpoint` placement modes | Only `auto` viewpoint spawn today |
| Structure `material_path` on ice walls | GeometryScript mesh has no material bind yet |

Agents requesting unsupported combinations receive `failed_validation` with
`next_tool` guidance — no silent fallback.
