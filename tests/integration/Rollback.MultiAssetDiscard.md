# Rollback.MultiAssetDiscard

**Owner:** WS-11. **Gates:** ADR-0005 `rollback.available` (must stay `false` until this passes).

## Automation test name

`UEREMCP.Validation.Rollback.MultiAssetDiscard`

Source: `Plugins/UEREMCP/Source/UeremcpValidation/Private/Tests/RollbackMultiAssetDiscard.spec.cpp`

## What it asserts

1. `FGlobalSandbox::Enter` succeeds.
2. Creating and `UPackage::Save`-ing N scratch `UCurveFloat` assets (concrete; `UDataAsset` is abstract) under `/Game/__UeremcpTests/Rollback_MultiAssetDiscard/` produces matching `GetChanges()` entries (**RB-06 q1**).
3. After deliberate failure → `Discard` → `Leave`:
   - no files under real `Content/__UeremcpTests/...` for those packages
   - `UEditorAssetLibrary::DoesAssetExist` is false
   - Asset Registry has no entry
   - `FindPackage` returns null
   (**RB-06 q3**)
4. Retry create outside the sandbox succeeds (editor still usable).

## Scratch / cleanup

- Suite: `Rollback_MultiAssetDiscard`
- Root: `/Game/__UeremcpTests/` only
- `FUeremcpScratchGuard` deletes the suite on scope exit even if the test fails

## How to run

```powershell
pwsh tests/run_editor_tests.ps1 -Filter "UEREMCP.Validation.Rollback.MultiAssetDiscard"
```

Requires `UeremcpValidation` registered and built (proposal: `docs/proposals/ws-11-register-validation-module.md`).

## Status

**Observed green** 2026-07-29 via `pwsh tests/run_editor_tests.ps1 -Filter UEREMCP.Validation`
(probe plugin; `-DisablePlugins=UEREMCP`). Log lines: `Q1 POSITIVE`, `Q3 POSITIVE`,
`Result={Success}` for both Smoke and MultiAssetDiscard.

See `docs/research/RB-06-sandbox-and-rollback.md` for scoped claims and remaining gaps
(BP compile, deletions, Config/Saved).
