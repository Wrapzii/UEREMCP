# Handoff: compile pass + from-scratch field test

Two documents. **Part A** is a prompt for compiling. **Part B** is four test
prompts to feed an agent one at a time against a new Unreal project.

Copy each block verbatim. Do not paste Part B all at once — the point is to see
where the agent goes wrong *without* being told what comes next.

---

# PART A — compile prompt

```
Two new UEREMCP tools were written without an editor available and have never
been compiled. Build them, fix what does not compile, and change nothing else.

Repo: main branch, commit da18335.

NEW FILES / EDITS
  Plugins/UEREMCP/Source/UeremcpEnvironment/Private/UeremcpMeshOps.cpp   (new)
  Plugins/UEREMCP/Source/UeremcpEnvironment/Public/UeremcpEnvironmentToolset.h
      -> adds SubmitMeshOps declaration
  Plugins/UEREMCP/Source/UeremcpEnvironment/UeremcpEnvironment.Build.cs
      -> adds "GeometryScriptingEditor"
  Plugins/UEREMCP/Source/UeremcpMaterial/Public/UeremcpMaterialToolset.h
      -> adds CreateMasterMaterial declaration
  Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialToolset.cpp
      -> adds CreateMasterMaterial implementation

WHAT IS ALREADY VERIFIED — do not "fix" these
  - UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox
    [VERIFIED: MeshPrimitiveFunctions.h:168]
  - UeremcpMaterialMasterBuilder::EnsureMasterMaterial and
    FUeremcpMaterialMasterBuildRequest{MasterPackagePath, Features, bTrailPurpose}
    [VERIFIED: UeremcpMaterialMasterBuilder.h]
  - Envelope pattern (ParseRequest / IsProtocolCompatible / MakeRejection /
    SerializeResponse / FUeremcpMutatingDispatch) is copied from the existing
    CreateProceduralTexture implementation in the same file.

WHAT IS UNVERIFIED — expect these to be where it breaks
  1. AppendCylinder, AppendCone, AppendSphereLatLong parameter lists were never
     read. Argument order or count may be wrong. Each lives in its own branch of
     ApplyOp() in UeremcpMeshOps.cpp so a mismatch is a local fix.
  2. CreateNewStaticMeshAssetFromMesh, its options struct
     FGeometryScriptCreateNewStaticMeshAssetOptions, and
     EGeometryScriptOutcomePins. All isolated in MakeStaticMeshAsset() in the
     same file. If the symbol lives in a different module than
     GeometryScriptingEditor, correct the Build.cs entry.
  3. Whether GeometryScriptingEditor is the right module name for an editor-only
     dependency here, and whether it needs a Target.bBuildEditor guard the way
     the existing "Water" dependency does at the bottom of that Build.cs.

RULES
  - Fix signatures to match the engine. Do NOT change behaviour to make
    something compile. In particular do not:
      * relax the empty-features rejection in create_master_material
      * make submit_mesh_ops skip an unsupported op instead of rejecting the
        whole request
      * upgrade any status to *_validated
    Those three are deliberate and are the point of both tools.
  - If a capability genuinely cannot be reached, leave the tool returning a
    rejection that says so. An honest failure is the correct output; a
    substitution is not.

AFTER IT BUILDS
  1. Launch the editor with the plugin enabled and confirm both tools appear:
       python tools/dump_tool_registry.py
     Expect UEREMCP tool count to go 44 -> 46.
  2. Re-run the checks, all must pass:
       python tools/check_tool_names.py          # expect 0 problems, 0 STALE
       python tools/check_operation_catalog.py   # expect 40 checked, 0 problems
       python tools/validate_schemas.py
       python tests/world_doc/test_reconcile.py
       python tests/world_doc/test_router_batch.py
       python tests/run_unit_tests.py
  3. Commit the snapshot refresh separately from any source fix.

REPORT BACK
  - Every signature that was wrong, with the correct one, so the [UNVERIFIED]
    tags in UeremcpMeshOps.cpp can be replaced with [VERIFIED: file:line].
  - Anything you had to change that was NOT a signature — that is a design
    problem and needs to be surfaced, not absorbed.
```

---

# PART B — field test, new empty Unreal project

Setup: a **new blank UE 5.8 project**, UEREMCP enabled, no content beyond
engine defaults. Give the agent MCP access and nothing else — **no repo access,
no docs, no hints.** That is the condition we are actually testing.

Feed these one at a time. Let each finish and capture the full transcript before
sending the next.

## Prompt 1 — cold start, deliberately vague

```
I have a brand new empty Unreal project. I want a small forest on a hillside:
a few different kinds of trees, and they should look like trees, not boxes.

Do it. Before you make any changes, tell me which tools you are going to use
and in what order, and why you picked those.
```

**What to watch for.** Does it find `ResolveIntent` at all, or start listing
toolsets? Does it get a `batch` back and use `ExecutePlan`, or issue one call
per domain? Does it discover it needs a mesh *before* it calls
`ScatterFoliage`, or after it gets cubes? Cubes are the failure we are hunting.

## Prompt 2 — the thing that used to be impossible

```
Now add a lake next to the forest, and a stone tower on the hill above it.

If anything you need does not exist or is not supported, stop and tell me
exactly what is missing rather than substituting something close.
```

**What to watch for.** `lake` must be refused, not silently built as a river.
`castle_keep`-style geometry must be refused or built from `submit_mesh_ops`,
not swapped for a box without saying so. This is the honesty test.

## Prompt 3 — iteration, the real cost model

```
The trees are too dense and too short, and I want the ones above the treeline
to have snow on them. Change just that. Do not rebuild the scene.
```

**What to watch for.** Does it re-derive everything from scratch, or modify
what exists? This is the question the world-document model exists to answer,
and right now there is no `read_world`, so expect it to struggle. **How** it
struggles is the useful data.

## Prompt 4 — the report we actually want

```
Stop building. Write me an engineering report on the tools themselves, based
only on what you just experienced. Be specific and be blunt — I am going to
use this to change the tooling, so vague praise is worthless.

Cover:

1. DISCOVERY. How many calls before your first successful mutation? What did
   you call that turned out to be wrong, and what made you think it was right?
   Name any tool whose description made you expect something it did not do.

2. BATCHING. Did anything tell you that you could send multiple dependent
   operations in one request? If yes, what and when. If no, at what point
   would that have helped most? Count your actual round trips and estimate
   what the minimum should have been.

3. HONESTY. List every case where a call reported success but the result was
   not what you asked for. Separately, list every case where it correctly told
   you something was unsupported. The second list is as important as the first.

4. DEAD ENDS. What did you want to do that you could not express at all? Not
   "it failed" -- what had no tool, no field, and no way to ask.

5. SCHEMAS. Which request schemas were guessable from the tool description
   alone, and which required trial and error? Quote the specific field you got
   wrong first.

6. NAMING. For each tool you used, would you have found it by searching for
   what you wanted in plain language? Which names actively misled you?

7. THE ONE CHANGE. If you could change exactly one thing about this toolset to
   make the next task faster, what is it and what would it save you? Be
   concrete -- name the tool and the change.

For every claim, cite the actual request you sent and the status you got back.
Do not summarise from memory.
```

---

## Recording results

For each prompt, keep:

- the full request/response transcript (statuses matter more than prose)
- total MCP round trips
- every `capability_notes` and `interpretation_notes` returned
- a screenshot or `CaptureWorldFrames` output of what was actually built

The gap between what the agent *believed* it built and what is actually in the
level is the highest-value signal in the whole exercise. It is the failure mode
that does not show up in any status code.

## Known-failing before you start

So these are not reported as discoveries:

- `create_water_body` ignores `body_type` entirely; lake and ocean silently
  yield a river [VERIFIED: never read in UeremcpEnvironmentService.cpp]
- `scatter_foliage` places `/Engine/BasicShapes/Cube` when `biome.mesh_path` is
  absent [VERIFIED: UeremcpEnvironmentService.cpp:1495]
- `place_structures` accepts only `ice_wall_ring|barrier_wall|box_along_river`
  [VERIFIED: UeremcpEnvironmentService.cpp:795] — it *rejects* unknown kinds,
  which is the correct behaviour and the model the other tools should follow
- no day/night or weather-cycle contract exists
- there is no `read_world`, so Prompt 3 has no good path yet

If the agent reports these, that is confirmation. If it reports them as
*successes*, that is the bug.
