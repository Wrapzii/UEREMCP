# Implementation spec — the 7 remaining Northridge items

**For:** whoever builds these next. **Status of the other 11:** see the status
table at the top of `RB-Northridge-api-shapes.md`.

Written to be built from, not discussed. Every engine API is tagged. Where I
could not read a header, it says `[UNVERIFIED]` and names what to check — two
of the last three defects in this codebase compiled cleanly and failed silently,
so treat an untagged claim as a bug waiting to happen.

**Build order matters here.** Item 1 is a shared dependency of items 2–4. Doing
it first turns four jobs into one plus three thin wrappers; doing it last means
building the same query four times.

---

## 1. `find_project_assets` — the AssetRegistry probe (MCP-012 + MCP-016)

**Why first:** MCP-008, MCP-010, MCP-012 and MCP-016 all need "what exists in
this project?" Nothing in UEREMCP asks the AssetRegistry today, which is why the
router handed out `/Game/Meshes/SM_Pine`, why agents never found
`/Game/PCG/SampleContent/SimpleForest`, and why import was invisible.

### API

```json
{"protocol_version":"1.0","action":"find_project_assets","request_id":"fa-1",
 "specification":{
   "roles":["foliage.tree","foliage.grass","structure.wall"],
   "class_filter":["StaticMesh"],
   "path_scopes":["/Game","/Engine/BasicShapes"],
   "max_per_role":5}}
```

`roles` are semantic; map each to name patterns in a **declared, data-driven**
table (put it in `operation_catalog.json`, not C++ — the catalog already drifted
twice when it lived in two places):

| role | patterns |
|---|---|
| `foliage.tree` | `*Tree*`, `*Pine*`, `*Conifer*`, `*Oak*`, `PCG_Tree*` |
| `foliage.grass` | `*Grass*`, `*Fern*`, `*Bush*` |
| `structure.wall` | `*Wall*`, `*Fence*`, `SM_*Wall*` |

### Response

```json
{"status":"no_change_required",
 "result":{"resolved":[{"role":"foliage.tree","matches":[
     {"path":"/Game/PCG/SampleContent/.../PCG_Tree_01","class":"StaticMesh","nanite":true}]}],
   "unresolved":[{"role":"foliage.grass","searched":["*Grass*","*Fern*"],
     "satisfied_by":{"action":"editor_toolset.toolsets.static_mesh.StaticMeshTools.import_file"}}]}}
```

**Rule:** `unresolved` is a first-class outcome, never an empty `resolved` entry.
The whole point is that an agent learns a role is unfillable *before* it scatters
cubes.

### Engine surface

```cpp
// [VERIFIED: Runtime/AssetRegistry/Public/AssetRegistry/AssetRegistryModule.h]
FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
// [VERIFIED: Runtime/AssetRegistry/Public/AssetRegistry/ARFilter.h] FARFilter
FARFilter Filter;
Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
Filter.PackagePaths.Add(FName("/Game"));
Filter.bRecursivePaths = true;
TArray<FAssetData> Found;
ARM.Get().GetAssets(Filter, Found);
```

`[UNVERIFIED]` Whether the registry has finished its initial scan. Check
`IAssetRegistry::IsLoadingAssets()` and return `partially_completed` with a note
rather than an empty result that reads as "nothing exists". **This is the failure
mode most likely to ship silently** — an early call returns nothing and the agent
concludes the project is empty.

`Build.cs`: add `"AssetRegistry"` to `UeremcpCore`.

### Acceptance

- A project containing PCG trees returns at least one `foliage.tree` match.
- An empty project returns `unresolved` for every role, never a fabricated path.
- Called during registry scan: `partially_completed` + a note, not `[]`.

---

## 2. `import_mesh_for_world` (MCP-008)

One call replacing import → unit-scale fight → collision preset → Nanite flag.

```json
{"action":"import_mesh_for_world",
 "target":{"asset_path":"/Game/__UeremcpPoc/Meshes/SM_Castle"},
 "specification":{
   "source_file":"C:/exports/castle.fbx",
   "source_unit":"meters",
   "collision":"complex_as_simple",
   "nanite":false,
   "expected_bounds_m":[240,145,90]}}
```

**`expected_bounds_m` is the load-bearing field.** Blender exports arrive at 1/100
scale constantly, and the failure is silent — a castle the size of a crate looks
fine in the outliner. Compare the imported bounds; if they differ from expected by
more than ~20%, **reject** with both numbers rather than importing something
unusable. If `expected_bounds_m` is omitted, return the actual bounds in
`structural_metrics` so the agent can check.

Compose over `StaticMeshTools.import_file` — do not reimplement FBX import.

`[UNVERIFIED]` `UStaticMesh::GetBoundingBox()` / `GetBounds()`, and the
`ECollisionTraceFlag::CTF_UseComplexAsSimple` assignment path on
`UBodySetup::CollisionTraceFlag`. Read `Engine/Classes/Engine/StaticMesh.h` and
`PhysicsEngine/BodySetup.h`.

### Acceptance
- A 100× undersized FBX with `expected_bounds_m` set → `rejected`, both numbers named.
- Without `expected_bounds_m` → succeeds, actual bounds in `structural_metrics`.
- `collision:"complex_as_simple"` → the mesh is walkable in PIE.

---

## 3. `place_prefab_on_landscape` (MCP-010)

Place + snap + clear foundation, as one operation. Every field-test floating
building came from doing these three separately and having one fail.

```json
{"action":"place_prefab_on_landscape",
 "specification":{
   "mesh_path":"/Game/__UeremcpPoc/Meshes/SM_Castle",
   "location_xy":[1800,-2000],
   "rotation_yaw":35,
   "clear_foliage_radius_cm":1500,
   "flatten_pad":{"radius_cm":1200,"falloff_cm":600}}}
```

Internally, in this order — the order is the specification:

1. Spawn at `(x, y, 0)`.
2. `SnapActorsToLandscape` logic (landscape-only trace) — **reuse
   `LandscapeZAt` in `UeremcpWorldOps.cpp`, do not write a second trace.**
3. `ClearFoliageInVolumes` over the mesh bounds expanded by
   `clear_foliage_radius_cm`.
4. Optional `flatten_pad` — heightmap write, the only genuinely new work.

Steps 1–3 are composition of shipped code. Ship those first and reject
`flatten_pad` as unsupported until step 4 exists; a partial tool that says so is
worth more than no tool.

`[UNVERIFIED]` Landscape heightmap editing for the pad:
`ALandscapeProxy::LandscapeEditorUtils` / `FLandscapeEditDataInterface::SetHeightData`.
Read `Editor/LandscapeEditor/` before attempting.

### Acceptance
- Placed prefab's pivot sits on terrain, not canopy — verified against a scene with foliage.
- No foliage instances remain within the radius.
- `flatten_pad` present while unimplemented → `partially_completed` naming it, everything else applied.

---

## 4. Landscape layer weight painting (MCP-004)

`CreateLandscapeMaterial` authors the blend and names the paint layers. Nothing
assigns weights, so the terrain renders as the first layer — the "white
landscape" complaint.

```json
{"action":"paint_landscape_layers",
 "specification":{"rules":[
   {"layer":"snow","min_height_m":1100,"blend_m":150},
   {"layer":"rock","min_slope_deg":35,"blend_deg":8},
   {"layer":"grass","fallback":true}]}}
```

Evaluate per-vertex: height and slope from the landscape's own data (**not a
recomputed heightmap — that is exactly the MCP-006 bug**), write weights, exactly
one `fallback` layer takes the remainder so weights sum to 1.

`[UNVERIFIED]` `ULandscapeComponent::GetLayerInfo`, `ALandscapeProxy::EditorLayerSettings`,
and the weightmap write path. This is the largest unknown in this document; budget
accordingly and read `Editor/LandscapeEditor/Private/LandscapeEdit.cpp` first.

### Acceptance
- Peaks above `min_height_m` render snow; slopes above `min_slope_deg` render rock.
- Weights sum to 1 per vertex.
- Reads the live landscape, never a recomputed surface.

---

## 5. `describe_operation` slim mode + cache (MCP-007)

~61 `describe_toolset` calls in one session; the Environment dump is ~83 KB of
duplicated nested envelope mirrors.

- `specification.detail`: `index` (names + one line each) | `slim` (default:
  required fields, statuses, one example) | `full` (today's behaviour).
- Return `content_hash` on every describe; accept `if_none_match` and reply
  `no_change_required` with an empty body when it matches.
- **Drop the duplicated `requestJson.contentSchema` mirror** — it is roughly half
  the payload and says nothing the envelope contract does not.

`next_actions.reference` (shipped) already removes most of the need. Measure
describe traffic after a build before investing more here.

### Acceptance
- `detail:index` for Environment is under 4 KB.
- Repeat describe with matching `if_none_match` returns no body.

---

## 6. Error contracts with `next_args` (MCP-011)

Rejections state a recipe in prose. Add the machine-readable form beside it —
prose costs a model a turn to parse and sometimes a wrong guess.

```json
{"status":"rejected",
 "error":{"code":"NONUNIFORM_SCALE",
   "message":"terrain.scale_xy (300) != terrain.scale_z (100)...",
   "next_args":{"specification":{"terrain":{"scale_xy":100,"scale_z":3}}}}}
```

`next_args` is a **patch to merge into the failing request**, not a whole
request. Start with the rejections that already know the fix: nonuniform scale,
needle `scale_z`, heightmap mismatch (MCP-006), realism gate, missing
`biome.mesh_path`. Add `error.code` to `response.schema.json` as an enum so codes
cannot be invented per site.

### Acceptance
- Every rejection carrying a recipe in prose also carries `next_args`.
- Merging `next_args` into the original request produces one that succeeds.

---

## 7. Not ours (MCP-013, MCP-014)

`user-unreal-watch` reports `serverStatus=error`; `SemanticSearch` returns 401.
Neither is UEREMCP code. Either fix the servers or remove them from
`GetStarted`'s advertised capabilities — an advertised dead tool costs an agent a
call and a wrong assumption every session. Recommend the capability probe (item 1
of the earlier plan) reports them as `available:false` with a fallback, rather
than staying silent.

---

## Ground rules for whoever builds these

1. **Read every engine header you call.** Two of the last three defects here —
   a missing `MATUSAGE_StaticLighting` and a `"Layer %s"` pin name — compiled
   cleanly and failed silently at runtime. The compiler is not verification.
2. **Tag every API claim** `[VERIFIED: file:line]` or `[UNVERIFIED]`. An untagged
   claim next to tagged ones implies it was checked. `bUsedWithLandscape` was
   invented, stated emphatically, shipped into a tool description, and caught by
   a compiler days later.
3. **Register plan handlers.** AICallable and plan-executable are separate
   registries; `check_operation_catalog.py` enforces it.
4. **Never silently substitute.** Refusal with a reason beats a plausible wrong
   result — every expensive failure in this project has been a call that
   succeeded and produced the wrong thing.
5. **Update both catalogs.** They are byte-compared; the drift check will fail.
6. **Refresh the snapshot** (`dump_tool_registry.py`) after the build, or the
   router cannot route to anything new.
