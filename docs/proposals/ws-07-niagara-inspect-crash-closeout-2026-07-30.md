# WS-07 Niagara post-create inspect crash closeout

- **Owner:** WS-07
- **Date:** 2026-07-30
- **Base:** local `main` `baa0d0663b8488cce6ec76746ae65ffad5fd79eb`
- **Audit artifact integrated:** `0099cb3` as `a4056b8`
- **Disposition:** UEREMCP now fails soft before `GetSystemSummary` when the
  loaded system or any renderer reference is invalid. The historical six-emitter
  create/post-create path no longer crashes on this branch, but its existing
  acceptance test still fails two unrelated honesty assertions. Direct Epic MCP
  attribution remains unresolved.

## Fix

`FUeremcpNiagaraInspect::Run` now performs a preflight before constructing the
external edit context or calling `GetSystemSummary`:

1. reject a `UNiagaraSystem` for which `IsValid` is false;
2. walk emitter data and reject any renderer for which `IsValid` is false;
3. return an honest error and failed `preflight_get_system_summary` trace entry;
4. after the utility call, reject any errors recorded by
   `FNiagaraExternalEditContext`.

This guard targets the unsafe dereference in Epic's summary walk:
`FillEmitterMetadata` checks for emitter data but calls `Renderer->GetClass()`
without checking each renderer pointer
`[VERIFIED: Engine/Plugins/FX/Niagara/Source/NiagaraEditor/Private/NiagaraExternalSystemEditorUtilities.cpp:1393-1402]`.
The utility itself checks only whether the system pointer is null before reading
the system and walking emitter handles
`[VERIFIED: Engine/Plugins/FX/Niagara/Source/NiagaraEditor/Private/NiagaraExternalSystemEditorUtilities.cpp:1409-1443]`.

The regression test inserts a null renderer into the transient in-memory state of
the dedicated `/Game/__UeremcpTests/NS_WS07_Probe` scratch asset, calls the real
UEREMCP inspect path, verifies a failed preflight diagnostic, and removes the null
entry without saving it.

## Live verification

### Build — PASS

Full `REEditor Win64 Development` build against the temporary branch junction
completed with exit code 0 and rebuilt the UEREMCP modules
`[VERIFIED-RUNTIME: Build.bat REEditor on 2026-07-30, 224 actions, Result: Succeeded]`.

### Focused inspect regression — PASS

`UEREMCP.Niagara.Inspect` ran five tests, all successful, including:

- `UEREMCP.Niagara.Inspect.InvalidRendererFailsSoft`
- `UEREMCP.Niagara.Inspect.NS_WS07_Probe`
- `UEREMCP.Niagara.Inspect.PathGuard`

Evidence:
`tests/integration/_logs/editor_UEREMCP_Niagara_Inspect_20260730_163656.log`
`[VERIFIED-RUNTIME: five Test Completed Result={Success} entries; editor exit 0]`.

### Historical six-emitter create/post-create path — NO CRASH, TEST FAIL

`UEREMCP.Niagara.POCB.SixEmitterGateScaffold` ran against the canonical
`/Game/__UeremcpTests/NS_POCB_FireballProbe` scaffold. The editor reached normal
automation completion; there was no access violation, fatal error, crash report,
or stranded Unreal process. The test result was still **FAIL**, not PASS, because
the response's `never_claims` array omitted `created_and_validated` and
`modified_and_validated`.

Evidence:
`tests/integration/_logs/editor_UEREMCP_Niagara_POCB_SixEmitterGateScaffold_20260730_163734.log:2985-3027`
`[VERIFIED-RUNTIME: Test Completed Result={Fail}; only assertions are
NiagaraPocBSixEmitterGate.spec.cpp:193 and :196; editor exited 255]`.

This closes the crash regression only. It does not convert the broader B7 gate to
PASS.

## Epic `GetSystemSummary`

Not called directly in this run. The direct Epic MCP wrapper remains unresolved.
The UEREMCP regression already exercised the shared NiagaraEditor utility on valid
systems, while the injected-invalid-renderer case was intentionally stopped by the
new preflight. A direct Epic call still needs the audit's requested isolated
throwaway project/watchdog and exact schema-shaped request before it is safe to
attempt.

## Remaining risk

- The guard covers invalid systems and invalid renderer references visible before
  the utility call. It cannot make arbitrary defects inside this experimental
  NiagaraEditor utility memory-safe.
- The six-emitter path completed without a crash once on this branch; this is a
  regression result, not a production stability claim.
- The broader six-emitter acceptance failure remains owned by its existing
  honesty-contract work; this fix does not change that response contract.
