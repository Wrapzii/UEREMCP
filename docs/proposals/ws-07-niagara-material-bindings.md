# WS-07: Niagara material-binding composition

- **From:** WS-07
- **To:** WS-07, WS-08, WS-01
- **Date:** 2026-07-30
- **Status:** proposal-only because `ws-07-niagara` currently has uncommitted enrich/round-trip work in the shared toolset, capability notes, inspect implementation, tests, and Niagara schema documentation

## Goal

Make one `create_niagara_effect` request resolve or create role materials, assign the
resulting `UMaterialInterface` paths to the matching emitter renderers, re-read those
renderer properties, and report unresolved or unverified work honestly. The agent
must not need a separate inspect → material create → Niagara mutate → inspect loop.

The existing Niagara specification already accepts
`materials.<role> = assetPath | {create_spec, reuse_if_present}`. No envelope or graph
schema change is required.

## Verified public surface

1. `FNiagaraExt_EmitterTopology::Renderers` returns renderer references containing
   renderer index and class.
   `[VERIFIED: $UE_ROOT/Engine/Plugins/FX/Niagara/Source/NiagaraEditor/Public/NiagaraExternalSystemEditorUtilities.h:867-905]`
2. `FNiagaraExt_StackItemReference` identifies a renderer by system, emitter name,
   and renderer index; `SetRenderer(int32)` is public.
   `[VERIFIED: $UE_ROOT/Engine/Plugins/FX/Niagara/Source/NiagaraEditor/Public/NiagaraExternalSystemEditorUtilities.h:943-1024]`
3. `GetEmitterTopology`, `GetRendererData`, and `SetRendererData` are public
   `UE_API` operations.
   `[VERIFIED: $UE_ROOT/Engine/Plugins/FX/Niagara/Source/NiagaraEditor/Public/NiagaraExternalSystemEditorUtilities.h:1262-1266,1306-1312,1374-1377]`
4. `FNiagaraExt_RendererData::PropertyValues` is the JSON property payload used by
   those renderer operations.
   `[VERIFIED: $UE_ROOT/Engine/Plugins/FX/Niagara/Source/NiagaraEditor/Public/NiagaraExternalSystemEditorUtilities.h:500-508]`
5. `SetRendererData` delegates to `SetAllObjectProperties`; renderer reads use
   `GetAllObjectProperties`.
   `[VERIFIED: $UE_ROOT/Engine/Plugins/FX/Niagara/Source/NiagaraEditor/Private/NiagaraExternalSystemEditorUtilities.cpp:1869-1882,2599-2604]`
6. Toolset reference JSON accepts either a string or
   `{"refPath":"Package.Object"}`, resolves/loads the object, validates its class,
   and assigns the object property.
   `[VERIFIED: $UE_ROOT/Engine/Plugins/Experimental/ToolsetRegistry/Source/ToolsetRegistry/Private/ToolsetRegistry/ReferenceConverter.cpp:191-243,299-418]`
7. Sprite and ribbon renderer properties expose editable `Material` object
   properties. A valid material user binding overrides the direct material.
   `[VERIFIED: $UE_ROOT/Engine/Plugins/FX/Niagara/Source/Niagara/Public/NiagaraSpriteRendererProperties.h:158-169]`
   `[VERIFIED: $UE_ROOT/Engine/Plugins/FX/Niagara/Source/Niagara/Public/NiagaraRibbonRendererProperties.h:241-253]`
8. Mesh renderers use editable `OverrideMaterials`; each
   `FNiagaraMeshMaterialOverride` has editable `ExplicitMat`, and the override is
   subordinate to `UserParamBinding`.
   `[VERIFIED: $UE_ROOT/Engine/Plugins/FX/Niagara/Source/Niagara/Public/NiagaraMeshRendererProperties.h:59-80,271-277]`

## WS-07-owned implementation

Limit the first implementation to existing material paths and material paths returned
by WS-08. Do not invent or edit a material graph in the Niagara module.

### Parsed model

Extend `FUeremcpNiagaraCreateSpec` with:

```cpp
struct FUeremcpNiagaraMaterialRequest
{
    FString Role;
    FString ExistingAssetPath;
    TSharedPtr<FJsonObject> CreateSpec;
    bool bReuseIfPresent = true;
};

TArray<FUeremcpNiagaraMaterialRequest> MaterialRequests;
```

Extend the result with:

```cpp
TMap<FString, FString> ResolvedMaterialPaths;
TArray<FString> RendererBindingsApplied;
TArray<FString> RendererBindingsVerified;
TArray<FString> UnresolvedMaterialBindings;
```

Every reported asset path must be the loaded object's canonical `GetPathName()`
(`Package.Object`), not the package-only request string.

### Binding algorithm

1. Resolve all direct paths to `UMaterialInterface` before creating the Niagara
   system. A missing asset or wrong class is an unresolved dependency, not a null
   renderer write.
2. Convert the role to the same emitter name used by create
   (`ribbon_trail` → `RibbonTrail`). Construct an emitter reference from the newly
   created system and that name.
3. Call `GetEmitterTopology` once per bound role and walk every renderer reference.
4. For sprite/ribbon renderer classes:
   - call `GetRendererData`;
   - parse `PropertyValues`;
   - set only `Material` to
     `{"refPath":"<canonical MI object path>"}`;
   - if `MaterialUserParamBinding` is valid, leave the renderer unchanged and record
     an unresolved conflict because that binding wins over `Material`.
5. For mesh renderer classes:
   - preserve all existing renderer JSON;
   - set `bOverrideMaterials=true`;
   - replace each existing `OverrideMaterials[*].ExplicitMat` with the canonical
     material reference while preserving `UserParamBinding`;
   - if there are no mesh slots, record the role as unresolved rather than claiming
     a binding.
6. Unsupported renderer classes (light, decal, volume, component, geometry cache)
   remain unchanged and are returned in `UnresolvedMaterialBindings`.
7. Call `SetRendererData` once per changed renderer. Any context error makes that
   renderer unverified and prevents a validated status.
8. Re-read with `GetRendererData` and compare each effective direct material field
   with the canonical object path. Add `niagara.material_bindings` to
   `checks_performed` only when every requested role has at least one matching,
   re-read-equal renderer and no winning user-material binding.
9. Compile after bindings, then save. Never bind after compile/save.

The helper that patches renderer JSON should be pure and separately unit tested.
It must preserve unknown renderer fields so this remains forward-compatible with
the complete-state renderer payload.

## WS-08 change required for inline creation

`UeremcpMaterialService::ExecuteCreateVfxMaterial` already returns
`PrimaryAsset` and honest status data, but its public declaration currently has no
module export macro.
`[VERIFIED: $UEREMCP_ROOT-ws08/Plugins/UEREMCP/Source/UeremcpMaterial/Public/UeremcpMaterialService.h:8-25]`

WS-08 should:

1. export `FUeremcpMaterialCreateResult` and
   `UeremcpMaterialService::ExecuteCreateVfxMaterial` with
   `UEREMCPMATERIAL_API`;
2. guarantee that `PrimaryAsset` is a package path accepted by
   `FSoftObjectPath` after a successful create/reuse result;
3. add a non-JSON service test proving another editor module can call the service
   and load `PrimaryAsset` as `UMaterialInterface`;
4. preserve its current honest status: a failed save remains
   `partially_completed`, not validated.
   `[VERIFIED: $UEREMCP_ROOT-ws08/Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialService.cpp:377-406]`

After that lands, WS-07 adds `UeremcpMaterial` as a private module dependency and,
for each `{create_spec,...}` entry, creates an internal `FUeremcpRequest` whose
target is deterministic:

```text
/Game/__UeremcpTests/Materials/MI_<NiagaraName>_<Role>
```

WS-07 must accept a created material only when WS-08 returns `bSuccess`, a
non-empty `PrimaryAsset`, and loading that path yields `UMaterialInterface`. The
WS-08 status and asset manifest must be copied into the Niagara response context;
it must not be collapsed to a boolean.

## WS-01 change required for atomic cross-domain composition

Calling the WS-08 service and then creating Niagara can leave a valid MI behind if
Niagara creation fails. Before this path can claim atomic validation, WS-01 should
run both internal domain operations inside the accepted `execute_plan` /
FileSandbox transaction boundary and merge both change manifests. Until that
orchestration lands:

- inline-created MI + failed Niagara creation reports `partially_completed`;
- the response names the retained MI in `created_assets`;
- no rollback claim is made;
- direct binding to an already-existing MI can proceed independently.

This does not require an envelope extension.

## Example request

```json
{
  "protocol_version": "1.0",
  "request_id": "poc-b-fireball-materials",
  "action": "create_niagara_effect",
  "target": {
    "asset_path": "/Game/__UeremcpTests/NS_POCB_Fireball"
  },
  "specification": {
    "name": "NS_POCB_Fireball",
    "effect_type": "projectile",
    "element": "fire",
    "components": ["core", "ribbon_trail"],
    "materials": {
      "core": "/Game/__UeremcpTests/Materials/MI_Fire_Core",
      "ribbon_trail": {
        "reuse_if_present": true,
        "create_spec": {
          "purpose": "elemental_projectile_trail",
          "element": "fire",
          "features": ["panning_textures", "erosion", "depth_fade", "dynamic_color"],
          "parameter_overrides": {
            "FlowSpeed": 1.2,
            "Turbulence": 0.9
          }
        }
      }
    },
    "parameters": {
      "primary_color": [1.0, 0.12, 0.01, 1.0],
      "secondary_color": [1.0, 0.75, 0.05, 1.0],
      "scale": 1.0,
      "intensity": 8.0
    }
  },
  "options": {
    "compile": true,
    "save": true,
    "validate": true
  }
}
```

## Honest status contract

- `rejected`: malformed role binding, disallowed path, or wrong asset class before
  any mutation.
- `failed_validation`: requested binding was written but renderer re-read differs,
  or compile finishes with errors.
- `partially_completed`: any role is unresolved, WS-08 leaves a created MI after
  later Niagara failure, compile times out, save is skipped/fails, structural
  re-read is skipped, or runtime smoke remains unexecuted.
- `created_with_warnings`: reserved for a saved, compiled, structurally re-read
  system where all requested renderer bindings re-read correctly but a
  non-binding warning remains.
- `created_and_validated`: prohibited until material re-read, structural re-read,
  compile, save/reload, dependency resolution, and the POC B runtime smoke gate all
  pass.

Merely returning from `SetRendererData` is not validation.

## Tests to land with implementation

Offline:

1. parse direct path and `create_spec` variants;
2. reject malformed material entries without mutation;
3. role-to-emitter mapping;
4. sprite/ribbon JSON patch preserves unrelated fields and emits `refPath`;
5. mesh JSON patch preserves user bindings and all unrelated fields;
6. winning `MaterialUserParamBinding` produces an unresolved conflict;
7. mixed resolved/unresolved roles force `partially_completed`;
8. schema fixture containing both material variants passes
   `tools/validate_schemas.py`.

Editor integration:

1. existing WS-08 core MI → sprite material set → renderer re-read equal;
2. existing WS-08 trail MI → ribbon material set → renderer re-read equal;
3. wrong-class path rejected before Niagara mutation;
4. user-material binding conflict remains partial;
5. compile errors prevent validated status;
6. save/reload preserves both renderer material paths.

No editor test should retarget the RE junction from a WS-07 worktree.
