# Proposal: Register `UeremcpValidation` in `UEREMCP.uplugin`

- **From:** WS-11
- **To:** WS-03 (owns `Plugins/UEREMCP/UEREMCP.uplugin`)
- **Date:** 2026-07-29
- **Status:** **Resolved for C-3** � uplugin + all four DLLs load in RE; shipping Validation gate green 2026-07-30 (see `tests/integration/_logs/editor_UEREMCP_Validation_shipping.redacted.md`).

## Ask (original)

Add an Editor module entry for `UeremcpValidation` to `UEREMCP.uplugin`, and ensure
FileSandbox is available (direct or via ToolsetRegistry).

Suggested module block — **now present** in the live RE plugin uplugin:

```json
{
  "Name": "UeremcpValidation",
  "Type": "Editor",
  "LoadingPhase": "Default",
  "TargetAllowList": [ "Editor" ]
}
```

FileSandbox plugin dependency also present. Thank you WS-03.

## Former shipping blocker (2026-07-29; cleared 2026-07-30)

`-KeepUeremcp -NoProbe` still aborts before Automation:

```text
Plugin 'UEREMCP' failed to load because module 'UeremcpProtocol' could not be found.
```

`UnrealEditor-UeremcpProtocol.dll` and `UnrealEditor-UeremcpValidation.dll` are
absent from `Plugins/UEREMCP/Binaries/Win64/` while Core/Transport DLLs exist.
Details: `tests/integration/_logs/shipping-gate-blocker.redacted.md`.

**Source of truth** for the Rollback test remains WS-11's tree:
`Plugins/UEREMCP/Source/UeremcpValidation/**` (do not fork the test body into the
probe). WS-03 may vendor a copy for the junction build; please prefer syncing from
ws-11 / main after merge so CurveFloat + C-3 docs stay current.

## Interim (honest)

Probe plugin is **launch-smoke only** (`UEREMCP.ValidationProbe.Launch.Smoke`).
Earlier probe Rollback green = **engine** FileSandbox evidence (C-3), not the
shipping UEREMCP gate.

```powershell
# Shipping (when Protocol+Validation DLLs load):
pwsh tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter "UEREMCP.Validation"

# Interim:
pwsh tests/run_editor_tests.ps1 -Filter "UEREMCP.ValidationProbe.Launch.Smoke"
```
