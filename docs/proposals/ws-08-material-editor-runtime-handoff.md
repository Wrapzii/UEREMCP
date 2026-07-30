# WS-08 → WS-11: Material CreateVfxMaterial editor runtime gate

- **From:** WS-08
- **To:** WS-11 / orch junction
- **Date:** 2026-07-30
- **Status:** ready for runtime proof

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
| `UeremcpMaterial.Toolset.CreateVfxMaterial.ProjectileCore` | `elemental_projectile_core` + fire + `boost_impact` → `created_and_validated` |
| `UeremcpMaterial.Toolset.CreateVfxMaterial.ProjectileTrail` | `elemental_projectile_trail` + ice + modifiers + `textures.FlowMap.generate` → `created_and_validated` |
| `UeremcpMaterial.Toolset.CreateProceduralTexture.Noise` | Standalone `create_procedural_texture` noise 128×128 → `created_and_validated` |

Scratch assets (auto-deleted by tests): `/Game/__UeremcpTests/Materials/MI_WS08_*`, masters under `/Game/__UeremcpTests/Materials/Masters/`, and `/Game/__UeremcpTests/Textures/T_*`.

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

## WS-08 local status

- Schema/unit tests pass without editor (`validate_schemas`, `test_specifications`, `test_element_presets`, `test_features`, `test_procedural_texture`).
- Editor automation tests implemented but **not executed** in WS-08 worktree (RE junction compile required).

## Not covered by runtime gate (still stubbed)

- `distortion`, `flow_maps`, `flipbook_subuv` feature tokens (graph wiring only — `FlowMap` texture slot binding works)
- Engine MaterialFunction composition
- Runtime JSON loader for `element_presets.v1.json` (C++ mirrors data)
- Purposes outside elemental projectile core/trail family
