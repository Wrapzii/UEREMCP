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
| `UeremcpMaterial.Toolset.CreateVfxMaterial.ProjectileTrail` | `elemental_projectile_trail` + ice + modifiers → `created_and_validated` |

Scratch assets (auto-deleted by tests): `/Game/__UeremcpTests/Materials/MI_WS08_*` and masters under `/Game/__UeremcpTests/Materials/Masters/`.

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

## WS-08 local status

- Schema/unit tests pass without editor (`validate_schemas`, `test_specifications`, `test_element_presets`).
- Editor automation tests implemented but **not executed** in WS-08 worktree (RE junction compile required).

## Not covered by runtime gate (still stubbed)

- `specification.features` → graph wiring
- `textures.generate` / procedural textures
- Full engine MaterialFunction composition
- Purposes outside elemental projectile core/trail family
