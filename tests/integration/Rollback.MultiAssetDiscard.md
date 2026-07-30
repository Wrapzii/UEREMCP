# Rollback.MultiAssetDiscard

**Owner:** WS-11. **Gates:** ADR-0005 shipping `rollback.available` for UEREMCP.

## Single source of truth

| Item | Path |
|---|---|
| Automation name | `UEREMCP.Validation.Rollback.MultiAssetDiscard` |
| **Implementation** | `Plugins/UEREMCP/Source/UeremcpValidation/Private/Tests/RollbackMultiAssetDiscard.spec.cpp` |
| Module | `UeremcpValidation` (must be registered in `UEREMCP.uplugin` — WS-03) |

The interim `UeremcpValidationProbe` plugin **does not** contain this test body
(C-3). Probe launch-smoke is unrelated to the shipping gate.

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

## How to run (shipping path)

```powershell
pwsh tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter "UEREMCP.Validation.Rollback.MultiAssetDiscard"
```

Requires `UeremcpValidation` registered and built (proposal: `docs/proposals/ws-11-register-validation-module.md`) and a loadable `UeremcpCore`.

## Status (C-3 nuance)

| Claim | Status |
|---|---|
| **Engine** FileSandbox q1/q3 (Content/ CurveFloat adds) | Observed green 2026-07-29 via interim probe path (`-DisablePlugins=UEREMCP`). Valid as engine semantics. |
| **Shipping** UEREMCP plugin gate (`UEREMCP.Validation.*`, UEREMCP enabled) | **Green** `[VERIFIED-RUNTIME: UnrealEditor-Cmd RE.uproject -KeepUeremcp -NoProbe -Filter UEREMCP.Validation 2026-07-30]` � `Rollback.MultiAssetDiscard` and `Harness.Smoke` `Result={Success}`. Log: `tests/integration/_logs/editor_UEREMCP_Validation_20260729_234458.log`. |

Prior blocker (missing Protocol/Validation DLLs): `tests/integration/_logs/shipping-gate-blocker.redacted.md`.

See `docs/research/RB-06-sandbox-and-rollback.md`.
