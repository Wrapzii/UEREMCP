# WS-08 → WS-07: Niagara material binding export

- **From:** WS-08
- **To:** WS-07
- **Date:** 2026-07-30
- **Status:** ready — satisfies WS-07 requirements in `ws-07-niagara-material-bindings.md` (`0f6378b`)

## Summary

`CreateVfxMaterial` already returns honest statuses, `PrimaryAsset`, and asset manifests.
This slice adds the **cross-module export surface** WS-07 needs to call the service
without JSON round-trips and to resolve deterministic MI paths for inline
`materials.<role>.create_spec` entries.

No envelope or Niagara schema changes required.

## Exported API (include `UeremcpMaterialNiagaraExport.h`)

| Symbol | Purpose |
|---|---|
| `UeremcpMaterialService::ExecuteCreateVfxMaterial` | `UEREMCPMATERIAL_API` — direct C++ entry (same logic as toolset) |
| `FUeremcpMaterialCreateResult` | `UEREMCPMATERIAL_API` result struct |
| `UeremcpMaterialNiagaraExport::ResolveMaterialInstancePath` | `/Game/__UeremcpTests/Materials/MI_<NiagaraName>_<Role>` |
| `UeremcpMaterialNiagaraExport::ResolvePurposeForNiagaraRole` | `core`/`ribbon_trail`/`*_material` → projectile purpose |
| `UeremcpMaterialNiagaraExport::BuildCreateVfxMaterialRequest` | `create_spec` JSON → `FUeremcpRequest` |
| `UeremcpMaterialNiagaraExport::ExecuteCreateVfxMaterialForNiagaraRole` | path + purpose merge + execute |
| `UeremcpMaterialNiagaraExport::VerifyPrimaryAssetIsMaterialInterface` | post-create load gate |

## WS-07 integration steps

1. Add `UeremcpMaterial` to `UeremcpNiagara` private dependencies.
2. For each `materials.<role>` with `{create_spec, reuse_if_present}`:
   - `Target = ResolveMaterialInstancePath(NiagaraAssetName, Role)`
   - `Result = ExecuteCreateVfxMaterialForNiagaraRole(NiagaraAssetName, Role, CreateSpec, …)`
   - Accept only when `Result.bSuccess && !Result.PrimaryAsset.IsEmpty()` and
     `VerifyPrimaryAssetIsMaterialInterface(Result.PrimaryAsset)` passes.
   - Copy `Result.Status`, `Result.CreatedAssets`, `Result.CapabilityNotes` into Niagara
     response — do not collapse to boolean.
3. Bind renderer `Material` to `PrimaryAsset` using canonical object path at bind time
   (WS-07-owned `SetRendererData` patch).

## PrimaryAsset contract

- **Format:** package path (`/Game/__UeremcpTests/Materials/MI_…`), `FSoftObjectPath`-valid.
- **Class:** loads as `UMaterialInterface` after `created_and_validated` / `modified_and_validated`.
- **Honest failure:** save failure → `partially_completed` (not validated); load verification
  failure → `failed_validation`.

## Role → purpose defaults

| Niagara `materials` role | Default `create_vfx_material.purpose` |
|---|---|
| `core`, `core_material` | `elemental_projectile_core` |
| `ribbon_trail`, `trail`, `trail_material` | `elemental_projectile_trail` |

When `create_spec.purpose` is present, it wins. Other roles must supply `purpose` explicitly.

## Shared MI parameters (Niagara renderer binding)

These names match WS-15 / Niagara user-param conventions documented in
`schemas/domains/materials/README.md`:

| Parameter | Type | Notes |
|---|---|---|
| `ParticleColor` | Vector | Primary tint |
| `ColorSecondary` | Vector | Gradient |
| `EmissiveScale` | Scalar | HDR intensity |
| `FlowSpeed` | Scalar | Pan / flow |
| `Turbulence` | Scalar | Noise scale |
| `SoftEdge` | Scalar | Radial falloff |
| `DepthFade` | Scalar | Camera fade |
| `DissolveAmount` | Scalar | Erosion |

Texture slots: `MainTexture`, `NoiseTexture`, `FlowMap`, `MaskTexture`.

## Editor test (WS-08)

`UeremcpMaterial.Service.NiagaraExport.CoreMaterial` — calls
`ExecuteCreateVfxMaterialForNiagaraRole` without JSON, verifies `PrimaryAsset` loads as
`UMaterialInterface`.

Run via WS-11 harness: `Filter "UeremcpMaterial.Service"`.

## Not WS-08-owned

- Renderer JSON patch + `SetRendererData` (WS-07)
- `execute_plan` / FileSandbox atomic rollback across domains (WS-01)
- Orch merge / RE junction compile (WS-03 / WS-11)

## Also in this slice

- **`flow_maps` feature token** wired in master graph (FlowMap sample × panned UV × emissive).
- Still stubbed: `distortion`, `flipbook_subuv`, runtime JSON loader for `element_presets.v1.json`.
