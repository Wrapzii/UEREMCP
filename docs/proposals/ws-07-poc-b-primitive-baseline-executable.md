# WS-07 → WS-14: executable POC-B primitive baseline

**Status:** One clean live trial passed; WS-14 repetition/archival remains
**Owner:** WS-07 (fixture); WS-14 (execution and metrics)
**Fixture:** `schemas/domains/niagara/fixtures/poc_b_primitive_baseline.py`

## What was missing and what is now fixed

WS-14's attempt artifact
`docs/reviews/metrics/artifacts/poc_b_primitive_baseline_attempt_20260730.json`
correctly rejected the prior outline because emitter refs, user-variable values,
renderer payloads, the material chain, compile termination, and save calls were not
executable inputs. The fixture now fixes each item:

- Six concrete emitter template object refs and semantic names match the role mapping
  used by the POC-B implementation. [VERIFIED:
  `Plugins/UEREMCP/Source/UeremcpNiagara/Private/UeremcpNiagaraRoleNames.cpp:23-38`]
- Immediately after cloning the system template, the fixture reads its system summary
  and removes every inherited emitter before adding the six role emitters. This
  self-corrects for `Minimal`, `Fountain`, or other residual template topology rather
  than assuming a particular template emitter name. `RemoveEmitter` takes a complete
  emitter stack-item reference and removes its associated scripts, modules, and
  renderers. [VERIFIED:
  `$UE_ROOT/Engine/Plugins/Experimental/Toolsets/NiagaraToolsets/Source/NiagaraToolsets/Private/NiagaraToolset_System.h:465-472`]
- The four `FNiagaraExt_UserVariable` wire payloads were re-read from the validated
  `/Game/__UeremcpPoc/NS_POCB_Fireball` asset. [VERIFIED-RUNTIME:
  `NiagaraToolsets.NiagaraToolset_System.GetUserVariables` on 2026-07-30]
- Renderer references come directly from each `AddEmitter` topology result. For every
  renderer the fixture performs one `GetRendererData`, patches its complete
  `propertyValues` payload, performs one `SetRendererData`, and later re-reads the
  binding. This avoids guessing renderer indices or dropping adjacent properties.
  [VERIFIED:
  `$UE_ROOT/Engine/Plugins/Experimental/Toolsets/NiagaraToolsets/Source/NiagaraToolsets/Private/NiagaraToolset_System.h:421-440,453-463`]
- Mesh material overrides are written with the live NiagaraToolsets wire key
  `explicitMat`. Reread accepts that key and case-tolerant aliases, including the
  native reflected property spelling `ExplicitMat`, while writes remove duplicate
  aliases and normalize back to `explicitMat`. The native UE property is
  `FNiagaraMeshMaterialOverride::ExplicitMat`. [VERIFIED:
  `$UE_ROOT/Engine/Plugins/FX/Niagara/Source/Niagara/Public/NiagaraMeshRendererProperties.h:59-75`]
  The lower-camel wire spelling was observed in the live mesh renderer payload.
  [VERIFIED-RUNTIME:
  `NiagaraToolsets.NiagaraToolset_System.GetRendererData` on
  `/Game/__UeremcpPoc/NS_POCB_Fireball_Baseline`, 2026-07-30]
- The material arm is fixed to six Epic
  `MaterialInstanceTools.create` calls. Each MI derives from a concrete, pre-UEREMCP
  project fireball/smoke/ribbon/impact Material and is bound to its matching role.
  All six parent assets resolved as `Material` in the target RE project.
  [VERIFIED-RUNTIME: batched `AssetTools.exists`, `get_asset_class`, and `load_asset`
  on 2026-07-30]
- Compile policy is exactly one blocking `GetSystemCompileState` after all mutations.
  It must return a non-stale, non-compiling, non-error accepted up-to-date state.
  The tool waits for in-flight compilation; no variable-count client poll loop is
  used. [VERIFIED:
  `$UE_ROOT/Engine/Plugins/Experimental/Toolsets/NiagaraToolsets/Source/NiagaraToolsets/Private/NiagaraToolset_System.h:577-584`;
  `$UE_ROOT/Engine/Plugins/FX/Niagara/Source/NiagaraEditor/Public/NiagaraExternalSystemEditorUtilities.h:200-211`]
- Save is one concrete `AssetTools.save_assets` call containing the system and all
  six MIs. Verification then checks all seven assets are not dirty.
  [VERIFIED-RUNTIME: `AssetTools` live schema discovery on 2026-07-30]

The script records every inner `execute_tool` call, including failures, in
`primitive_trace`; `primitive_ops_executed` is `len(primitive_trace)`. It does not
contain a planned or fabricated count. Template cleanup contributes one
`GetSystemSummary` operation plus one `RemoveEmitter` operation per inherited emitter,
so consumers must use the returned count rather than expecting the failed run's 36.

## Fixed semantic inputs

| Role | Emitter template | Material parent |
|---|---|---|
| core | `/Niagara/DefaultAssets/Templates/Emitters/Minimal.Minimal` | `/Game/RE/VFX/Magecraft/RuntimeMaterials/M_FX_FireboltCore.M_FX_FireboltCore` |
| flame_shell | `/Niagara/DefaultAssets/Templates/Emitters/UpwardMeshBurst.UpwardMeshBurst` | `/Game/RE/VFX/Magecraft/RuntimeMaterials/M_FX_FireboltShell.M_FX_FireboltShell` |
| sparks | `/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst.SimpleSpriteBurst` | `/Game/RE/VFX/Magecraft/RuntimeMaterials/M_FX_FireboltSpark.M_FX_FireboltSpark` |
| smoke | `/Niagara/DefaultAssets/Templates/Emitters/Fountain.Fountain` | `/Game/RE/VFX/Magecraft/RuntimeMaterials/M_Niagara_Smoke.M_Niagara_Smoke` |
| ribbon_trail | `/Niagara/DefaultAssets/Templates/Emitters/LocationBasedRibbon.LocationBasedRibbon` | `/Game/RE/VFX/Magecraft/RuntimeMaterials/M_Niagara_FireRibbon.M_Niagara_FireRibbon` |
| impact_burst | `/Niagara/DefaultAssets/Templates/Emitters/OmnidirectionalBurst.OmnidirectionalBurst` | `/Game/RE/VFX/Magecraft/RuntimeMaterials/M_Niagara_ImpactFlash.M_Niagara_ImpactFlash` |

The baseline uses specialized project-native fireball role materials as its
pre-UEREMCP primitive equivalent. It does not invoke WS-08 goal operations and does
not count UEREMCP material internals. This choice fixes the material call grain at one
Epic MI creation primitive per role while preserving six role-specific material
assets and bindings.

## Clean-state requirement

The fixture never deletes. Before mutation it checks these exact outputs and returns
`blocked_dirty_target` if any exists:

```text
/Game/__UeremcpPoc/NS_POCB_Fireball_Baseline
/Game/__UeremcpPoc/Materials/MI_NS_POCB_Fireball_Baseline_core
/Game/__UeremcpPoc/Materials/MI_NS_POCB_Fireball_Baseline_flame_shell
/Game/__UeremcpPoc/Materials/MI_NS_POCB_Fireball_Baseline_sparks
/Game/__UeremcpPoc/Materials/MI_NS_POCB_Fireball_Baseline_smoke
/Game/__UeremcpPoc/Materials/MI_NS_POCB_Fireball_Baseline_ribbon_trail
/Game/__UeremcpPoc/Materials/MI_NS_POCB_Fireball_Baseline_impact_burst
```

WS-14 should clean only those controlled benchmark outputs before each trial using
its existing clean-state procedure. A blocked preflight or failed validation is not
a completed trial.

## Exact invocation

1. Read the fixture file as text.
2. Start the client monotonic timer immediately before this single MCP call:

```json
{
  "toolset_name": "editor_toolset.toolsets.programmatic.ProgrammaticToolset",
  "tool_name": "execute_tool_script",
  "arguments": {
    "script": "<entire contents of schemas/domains/niagara/fixtures/poc_b_primitive_baseline.py>"
  }
}
```

3. Stop the client monotonic timer immediately after the call returns.
4. Parse `returnValue` as JSON. A usable trial must satisfy all of:

```text
status == "created_and_validated"
completed == true
primitive_ops_executed == len(primitive_trace)
all primitive_trace[*].ok == true
removed_template_emitters contains every inherited template emitter
len(emitters) == 6
len(user_variables) == 4
len(renderer_bindings_verified) >= 6
compile_state.bIsCompiling == false
compile_state.bIsStale == false
compile_state.bHasErrors == false
```

Record:

- outer MCP round trips: `1`
- primitive operations: returned `primitive_ops_executed`
- client wall clock: the outer monotonic interval, not `inner_elapsed_ms`
- completion: returned `completed`
- raw result and full `primitive_trace`

`inner_elapsed_ms` is diagnostic only. Repeat from clean state at least three times,
as required by the original handoff. Do not report a reduction ratio from a failed,
blocked, or partial run.

## Live trial status and remaining handoff

Three clean trials after inherited-emitter removal reached 42 primitive operations
each, then failed mesh renderer reread because the fixture expected `ExplicitMat`
instead of the live `explicitMat` wire key. Those are failed trials, not baseline
measurements. [VERIFIED-RUNTIME:
`docs/reviews/metrics/artifacts/poc_b_primitive_baseline_post_04b3541_failed_20260730.json`]

After correcting the wire key, one clean live trial passed all harness acceptance
checks with `status == "created_and_validated"`, 63 executed primitive operations,
and 6.4099332 seconds client wall clock. It removed the inherited `Minimal` emitter,
reread all six renderer bindings including `FlameShell`, reported `UpToDate` with no
compile errors, and left all seven saved outputs clean. [VERIFIED-RUNTIME:
`python docs/reviews/metrics/run_poc_b_primitive_live.py --fixture
schemas/domains/niagara/fixtures/poc_b_primitive_baseline.py --output
%TEMP%/poc_b_primitive_baseline_ws07_fixed_20260730.json --trials 1
--execute-cleanup`, 2026-07-30]

WS-14 should repeat from clean state for the requested three-trial metrics set and
archive the raw responses under its owned metrics path. The exact rerun command is:

```powershell
python docs/reviews/metrics/run_poc_b_primitive_live.py `
  --fixture schemas/domains/niagara/fixtures/poc_b_primitive_baseline.py `
  --output docs/reviews/metrics/artifacts/poc_b_primitive_baseline_fixed_20260730.json `
  --trials 3 `
  --execute-cleanup
```

This handoff does not claim overall POC-B.
