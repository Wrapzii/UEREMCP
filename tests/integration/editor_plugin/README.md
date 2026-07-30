# UeremcpValidationProbe (interim)

**Owner:** WS-11. **Not** the shipping rollback gate.

| What | Where |
|---|---|
| **Source of truth** for `Rollback.MultiAssetDiscard` | `Plugins/UEREMCP/Source/UeremcpValidation/Private/Tests/RollbackMultiAssetDiscard.spec.cpp` |
| This probe | Launch-smoke only: `UEREMCP.ValidationProbe.Launch.Smoke` |

## C-3 nuance (WS-01 accepted)

Earlier probe runs that exercised `FGlobalSandbox` remain valid evidence of **engine
FileSandbox semantics**. They do **not** prove the shipping UEREMCP plugin path
(`UeremcpValidation` loaded via `UEREMCP.uplugin` with Core present).

Use this probe only when UEREMCP cannot load (missing `UeremcpCore` binary, or
Validation not registered). Prefer:

```powershell
pwsh tests/run_editor_tests.ps1 -Filter "UEREMCP.Validation" -KeepUeremcp
```

once WS-03 registers `UeremcpValidation` and Core links. See
`docs/proposals/ws-11-register-validation-module.md`.
