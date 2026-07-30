# WS-08 → WS-07: POC MI path co-location fix

- **From:** WS-08
- **To:** WS-07
- **Date:** 2026-07-30
- **Status:** WS-08 API landed; **WS-07 wiring required** for fireball runtime

## Diagnosis (fireball FAIL)

Niagara system: `/Game/__UeremcpPoc/NS_POCB_Fireball`

MIs created at: `/Game/__UeremcpTests/Materials/MI_NS_POCB_Fireball_*`

**Root cause:** WS-07 inline material binding calls WS-08 legacy helpers that hard-default to `TestsContentRoot`:

| Call site | Current (wrong for POC) | Should use |
|---|---|---|
| `UeremcpNiagaraMaterialBinding.cpp:342` | `ResolveMaterialInstancePath(NiagaraAssetName, Role)` | `ResolveMaterialInstancePathForNiagaraSystem(NiagaraSystemPackagePath, Role)` |
| `UeremcpNiagaraMaterialBinding.cpp:387` | `ExecuteCreateVfxMaterialForNiagaraRole(NiagaraAssetName, ...)` | `ExecuteCreateVfxMaterialForNiagaraSystem(NiagaraSystemPackagePath, ...)` |
| `UeremcpNiagaraCreate.cpp:367` | `ResolveMaterialPaths(AssetName, ...)` | `ResolveMaterialPaths(Request.TargetAssetPath, ...)` |

`a26090f` added path resolution for POC targets, but **did not wire the Niagara execute entry point**. WS-08 commit after this proposal adds `ExecuteCreateVfxMaterialForNiagaraSystem`.

## WS-08 API (correct for POC)

```cpp
// Path only
ResolveMaterialInstancePathForNiagaraSystem(
    TEXT("/Game/__UeremcpPoc/NS_POCB_Fireball"), TEXT("core"));
// → /Game/__UeremcpPoc/Materials/MI_NS_POCB_Fireball_core

// Inline create (honors caller target scratch root + co-located masters)
ExecuteCreateVfxMaterialForNiagaraSystem(
    NiagaraSystemPackagePath, Role, CreateSpec, bCompile, bValidate, bSave, bDryRun);
```

Direct MCP `create_vfx_material` with `target.asset_path` under `/Game/__UeremcpPoc/...` already works — no WS-08 change needed.

`ExecuteCreateVfxMaterialForNiagaraRole(NiagaraAssetName, ...)` remains **tests-root automation only** (`/Game/__UeremcpTests/Materials/...`).

## WS-07 required diff (owned paths)

1. **`UeremcpNiagaraMaterialBinding.h/.cpp`** — rename first parameter of `ResolveMaterialPaths` from `NiagaraAssetName` to `NiagaraSystemPackagePath` (package path, not bare name).

2. **`UeremcpNiagaraCreate.cpp`** — pass `Request.TargetAssetPath` (or composed package path) instead of `Spec.Name` / `AssetName`.

3. **Inline create/reuse paths** — switch to system-path APIs above; `reuse_if_present` probe path must match the POC MI path.

4. **`IsAllowedMaterialProbePath`** — replace hardcoded `GMaterialsProbeRoot = "/Game/__UeremcpTests/Materials"` with `UeremcpNiagaraPaths::IsAllowedProbePath` (already allows `__UeremcpPoc`).

## Verification

After WS-07 lands: fireball single-call response should list MIs under `/Game/__UeremcpPoc/Materials/` only; `NiagaraPocBFireballMaterials.spec` should stop flagging `/Game/__UeremcpTests` in summary.

## No claim

WS-08 fix alone does not unblock fireball — WS-07 must call the system-path execute API.
