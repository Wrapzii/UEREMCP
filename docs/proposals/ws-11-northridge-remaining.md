# Plan — the six Northridge items not yet fixed

**Raised by:** WS-11
**Date:** 2026-07-31
**Source:** Northridge field-test backlog (RB-Northridge)
**Fixed already:** #1 staging wipe (`c3ac0dd`), #4 scale, #5 realism gate,
#9 `CreateLandscapeMaterial` (`6edf07e`)

Ordered by value per unit of risk. Items 1–3 are contained changes to code that
exists. Item 4 is new measurement. Item 5 I recommend **rejecting** as specified.

---

## 1. Capability probe and offline fallbacks (#10 / P0-6)

**Smallest, and it unblocks the others.** The agent found Blender PolyHaven,
Sketchfab and Hyper3D disabled, concluded there was no path to real assets, and
shipped primitives. Meanwhile `StaticMeshTools.import_file` existed, and the
project already contained `/PCG/SampleContent/SimpleForest` trees it never
found.

Extend `GetStarted` with `specification.detail = "capabilities"`:

```json
"capabilities": {
  "asset_sources": [
    {"name": "StaticMeshTools.import_file", "available": true,
     "note": "import any FBX/OBJ/GLTF already on disk"},
    {"name": "blender_polyhaven", "available": false,
     "fallback": "fetch over HTTP and import_file"}
  ],
  "project_assets": {
    "foliage": ["/Game/PCG/SampleContent/.../PCG_Tree_01"],
    "materials": [], "meshes_total": 47
  },
  "unsupported": ["hydrology.ocean", "beach", "roads", "city_layout"],
  "map_state": {"dirty": true, "untitled": true,
                "recovery": "save_as before build_environment switches maps"}
}
```

`project_assets` comes from an AssetRegistry query at call time. **Never a
hardcoded list** — that is the drift trap the catalog already fell into twice.

**Why first:** it is a read-only probe, it cannot break anything, and every
other item benefits from an agent that knows what exists.

**Acceptance:** in a project containing PCG trees, a cold `GetStarted` names at
least one foliage mesh. With no foliage anywhere, `project_assets.foliage` is
`[]` and `asset_sources` still names `import_file`.

---

## 2. Live asset resolution in the router (#2 / #6 / P0-2)

The router hands out asset paths it has never checked. `/Game/Meshes/SM_Pine` is
gone, but the class of bug is not: nothing verifies that any path in a plan
exists.

Add an AssetRegistry probe to `ResolveIntent`, and a `required_assets` block on
the response:

```json
"asset_resolution": {
  "resolved":   [{"role": "foliage.tree", "path": "/Game/PCG/.../PCG_Tree_01",
                  "found_by": "asset_registry"}],
  "missing":    [{"role": "structure.castle", "searched": ["*Castle*", "*Keep*"],
                  "satisfied_by": {"action": "editor_toolset...import_file"}}],
  "unsupported": ["hydrology.ocean"]
}
```

**The rule that matters:** a path may only appear inside an executable
`request_json` if the AssetRegistry confirms it. Anything unresolved goes in
`missing` with the action that would fix it — never inline where an agent will
send it and get a cube.

This shares its probe with item 1. Build them together.

**Acceptance:** every asset path in any emitted `request_json` resolves, or the
plan is rejected. A fantasy-region intent in an empty project returns
`missing.foliage.tree`, not a phantom path.

---

## 3. Domain services honour `options.on_unsupported` (#7)

The option parses today and no domain branches on it. That is a half-built
contract, and half-built is worse than absent: an agent that sets
`partial` and gets all-or-nothing behaviour has been lied to.

Environment first, since that is where multi-part asks land. In
`FUeremcpEnvironmentService::Build`, when a stage is unsupported:

- `on_unsupported=fail` (default) → reject the whole request, as now.
- `on_unsupported=partial` → apply the supported stages, skip the rest,
  return `partially_completed` with `refused[]` naming each skipped stage and
  why.

**Acceptance:** a request for terrain + foliage + ocean with `partial` builds
terrain and foliage, and returns `refused: [{stage: "ocean", reason: "Phase 2"}]`.
The same request with `fail` changes nothing at all.

---

## 4. Scene-health validation (#8 / P0-4)

`ValidateEnvironment` checks a MountainRiverRain checklist. It cannot see
floating trees, water that misses the terrain, or a scene that is all cliff —
every defect visible in the field-test screenshots.

The slope histogram and `flat_area_pct` from `6edf07e` are the first two
measurements. Add:

| Gate | Measurement |
|---|---|
| `max_foliage_snap_error_cm` | per-instance distance to the landscape below it |
| `max_water_terrain_gap_cm` | sampled along the water spline |
| `forbid_basicshape_foliage` | any HISM whose mesh is `/Engine/BasicShapes/*` |
| `require_uniform_landscape_scale` | already computed, just gate it |
| `min_flat_area_pct` | already computed, just gate it |

Selectable by `specification.profile` so the legacy MRR gates keep working.

**Why fourth:** the highest-value ones (`snap_error`, `basicshape_foliage`) are
cheap because the actors are already enumerated. Water gap needs spline
sampling and is the only real work.

**Acceptance:** a fixture with foliage raised 500cm fails the snap gate; cube
foliage fails `forbid_basicshape_foliage`; the MRR profile is unchanged.

---

## 5. Starter-region template (#3) — recommend REJECT as specified

Their proposal is one `InstantiateTemplate` producing "fantasy region: flats +
mountains + beach + 3 settlements + roads + castle."

**This is the pre-baked library this project decided not to ship.** The stated
principle is that the MCP gives an agent the tools to build its own library per
game, and ships none itself. A "fantasy region" template is that library with
one entry, and the next request will be for a desert region, then a city, then a
space station — each a new hardcoded outcome. It is the cubes-for-trees defect
promoted to a feature: enumerate the anticipated scenario, degrade everything
else.

It is also the exact shape the subsystem audit called a script rather than a
subsystem.

**What they actually needed, and should get instead:** the stages that do not
exist. Their scene failed because there is no coast, no road, and no settlement
pad — so the agent placed cubes. Three composable stages, not one template:

- `create_coastline` — a shore heightmap band plus a water body, honouring
  `body_type` at last
- `create_road_spline` — a spline that conforms to terrain and carves a flat
  corridor
- `create_settlement_pad` — a flattened, slope-gated area sized for buildings

Each is independently useful, each composes with the rest, and none anticipates
"fantasy". Then `promote_to_template` lets an agent bank *its own* Northridge
once it works — which is the library story we already agreed on and which is
already built apart from the security wire (Part VII.4).

**If the counter-argument is "agents need a worked example to copy":** that is a
documentation problem, and the answer is a worked `execute_plan` in the docs, not
a shipped asset.

---

## Sequencing

1. Capability probe (#10) — read-only, unblocks everything
2. Asset resolution (#2/#6) — shares the probe
3. `on_unsupported` in Environment (#7) — finishes a contract already half-shipped
4. Scene-health gates (#8) — two are nearly free, one is real work
5. Coast / road / settlement stages — instead of #3, and the largest job

**Before any of it: get a build in.** Everything from `c3ac0dd` onward is
source-only; deploy sits at `71c4506`. Four fixes are already stacked unverified,
including `CreateLandscapeMaterial`, whose `UMaterialEditingLibrary` signatures
were never read. Adding five more items on top of an unbuilt pile is how a single
early compile error invalidates a week.
