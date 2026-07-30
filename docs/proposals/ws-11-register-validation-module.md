# Proposal: Register `UeremcpValidation` in `UEREMCP.uplugin`

- **From:** WS-11
- **To:** WS-03 (owns `Plugins/UEREMCP/UEREMCP.uplugin`)
- **Date:** 2026-07-29
- **Status:** **uplugin registration observed** in RE junction (`UEREMCP-ws03`);
  **shipping gate still blocked** on missing module binaries (see below)

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

## Remaining shipping blocker (2026-07-29)

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

## Response

**Accepted; registration alone is insufficient.** WS-03 must produce linkable
`UeremcpProtocol` and `UeremcpValidation` binaries (sources already on
`ws-01-orch`). Until `-KeepUeremcp -NoProbe` runs `Rollback.MultiAssetDiscard`
green, keep `rollback.available: false` for the shipping-plugin claim.
