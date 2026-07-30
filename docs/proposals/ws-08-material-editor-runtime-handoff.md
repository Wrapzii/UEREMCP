# WS-08 → WS-11: Material CreateVfxMaterial editor runtime gate

- **From:** WS-08
- **To:** WS-11 / orch junction
- **Date:** 2026-07-30
- **Status:** runtime proof blocked before automation discovery

## What to run

On RE project with orch junction plugin (`UEREMCP-ws01`) compiled after this commit:

```powershell
pwsh tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter "UeremcpMaterial.Toolset"
```

Expected green:

| Test | Proves |
|---|---|
| `UeremcpMaterial.Toolset.Echo` | Envelope parse without asset mutation |
| `UeremcpMaterial.Toolset.Register` | ToolsetRegistry schema for material toolset |
| `UeremcpMaterial.Toolset.CreateVfxMaterial.ProjectileCore` | `elemental_projectile_core` + fire + `boost_impact` → `created_and_validated` when proof succeeds; else honest `partially_completed` |
| `UeremcpMaterial.Toolset.CreateVfxMaterial.ProjectileTrail` | `elemental_projectile_trail` + ice + modifiers + `textures.FlowMap.generate` → `created_and_validated` when proof succeeds |
| `UeremcpMaterial.Toolset.CreateProceduralTexture.Noise` | Standalone `create_procedural_texture` noise 128×128 → `created_and_validated` when dimension re-read succeeds |
| `UeremcpMaterial.Service.NiagaraExport.CoreMaterial` | Direct C++ `ExecuteCreateVfxMaterialForNiagaraRole` + `VerifyPrimaryAssetIsMaterialInterface` |

Scratch assets (auto-deleted by tests): `/Game/__UeremcpTests/Materials/MI_WS08_*`, `/Game/__UeremcpTests/Materials/MI_NS_WS08_ExportProbe_core`, masters under `/Game/__UeremcpTests/Materials/Masters/`, and `/Game/__UeremcpTests/Textures/T_*`.

## APIs exercised (verification tags)

| API | Tag |
|---|---|
| `UMaterialEditingLibrary::CreateMaterialExpression` | `[VERIFIED: MaterialEditingLibrary.h:168]` |
| `UMaterialEditingLibrary::ConnectMaterialExpressions` | `[VERIFIED: MaterialEditingLibrary.h:242]` |
| `UMaterialEditingLibrary::ConnectMaterialProperty` | `[VERIFIED: MaterialEditingLibrary.h:232]` |
| `UMaterialEditingLibrary::RecompileMaterial` | `[VERIFIED: MaterialEditingLibrary.h:267]` |
| `UMaterialEditingLibrary::SetMaterialInstance*ParameterValue` | `[VERIFIED: MaterialEditingLibrary.h:421-457]` |
| `IAssetTools::CreateAsset` + `UMaterialFactoryNew` | `[VERIFIED: material.py:44-46]` |
| `UMaterialInstanceConstantFactoryNew::InitialParent` | `[VERIFIED: MaterialInstanceConstantFactoryNew.h:20]` |
| `UEditorAssetSubsystem::SaveAsset` / `DoesAssetExist` | `[VERIFIED: REAgentTools material_workflow_tools.py:67]` |
| `FImageUtils::CreateTexture2D` | `[VERIFIED: ImageUtils.h:268]` |
| `FMath::PerlinNoise2D` | `[VERIFIED: UnrealMathUtility.h:2472]` |
| `UMaterialExpressionTextureSampleParameter2D` | `[VERIFIED: MaterialExpressionTextureSampleParameter2D.h]` |

## Editor filter triage — 2026-07-30 (WS-01 `ws-01-editor-filter-results.md`)

**Diagnosis:** `-NullRHI` automation runs exposed two honesty bugs, not false `*_validated` claims:

1. **Fresh master/MI assets:** `EditorAssetSubsystem::LoadAsset` failed immediately after create because `FAssetRegistryModule::AssetCreated` was not called on masters/MIs. Service returned honest `failed_validation` ("Failed to load master after ensure") while tests expected `created_and_validated`.
2. **Texture dimension proof:** `UTexture2D::GetSizeX/Y` returns `0` under NullRHI even when `FTextureSource` holds the correct CPU dimensions. Validation correctly refused `created_and_validated`; fix reads `Source.GetSizeX/Y` first and caps at `partially_completed` when proof is unavailable.

**Fix (WS-08 `dc6eed5+`):** register fresh assets, reuse in-process `UMaterial*` from master build, dimension proof via `FTextureSource`, in-process MI fallback for PrimaryAsset verify, editor tests accept `partially_completed` when JSON never claims `*_validated`.


- Schema/unit tests pass without editor (`validate_schemas`, `test_specifications`, `test_element_presets`, `test_features`, `test_procedural_texture`).
- Editor automation tests implemented but **not executed** in WS-08 worktree (RE junction compile required).

## RE runtime attempt — 2026-07-30

Preconditions were checked before launch:

- RE `Plugins/UEREMCP` was a junction to `UEREMCP-ws01/Plugins/UEREMCP`.
- Orch contained `CreateVfxMaterial` wiring as `acd75dc`, equivalent to WS-08
  `4ade2ae`.
- No Unreal Editor or Live Coding process was active.
- A forced targeted build with `-Module=UeremcpMaterial -NoUBTMakefiles
  -NoHotReloadFromIDE -WaitMutex` compiled and linked
  `UnrealEditor-UeremcpMaterial.dll`.
  `[VERIFIED-RUNTIME: RE UBT targeted build exited 0 on 2026-07-30]`

The required command was launched twice. The first launch stopped during plugin load
because `UeremcpMaterial` had no binary. After the successful forced targeted build,
the second launch got past that module but stopped during plugin load because
`UeremcpNiagara` had no loadable binary.
`[VERIFIED-RUNTIME: tests/integration/_logs/editor_UeremcpMaterial_Toolset_20260730_012857.log]`

No material automation test was discovered or executed:

| Test | Result |
|---|---|
| `UeremcpMaterial.Toolset.Echo` | **NOT RUN** — UEREMCP plugin startup blocked |
| `UeremcpMaterial.Toolset.Register` | **NOT RUN** — UEREMCP plugin startup blocked |
| `UeremcpMaterial.Toolset.CreateVfxMaterial.ProjectileCore` | **NOT RUN** — UEREMCP plugin startup blocked |
| `UeremcpMaterial.Toolset.CreateVfxMaterial.ProjectileTrail` | **NOT RUN** — UEREMCP plugin startup blocked |
| `UeremcpMaterial.Toolset.CreateProceduralTexture.Noise` | **NOT RUN** — UEREMCP plugin startup blocked |
| `UeremcpMaterial.Service.NiagaraExport.CoreMaterial` | **NOT RUN** — UEREMCP plugin startup blocked |

Honest runtime status: **partially_completed**. The Material module itself compiled
and linked, but `created_and_validated` remains unverified for both
`CreateVfxMaterial` cases.

### Blockers observed

- Current orch cannot produce a complete loadable UEREMCP plugin. A full REEditor
  build failed in non-WS-08 modules, including Gameplay, Niagara, and Templates.
- Because UEREMCP declares all of those editor modules, one missing module prevents
  automation discovery even when `UeremcpMaterial` is loadable.
- Re-run the required command after the orch all-modules build is green; do not treat
  this attempt as a failed material test.

## Not covered by runtime gate (still stubbed)

- `distortion`, `flow_maps`, `flipbook_subuv` feature tokens (graph wiring only — `FlowMap` texture slot binding works)
- Engine MaterialFunction composition
- Runtime JSON loader for `element_presets.v1.json` (C++ mirrors data)
- Purposes outside elemental projectile core/trail family
