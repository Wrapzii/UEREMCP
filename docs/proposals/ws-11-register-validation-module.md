# Proposal: Register `UeremcpValidation` in `UEREMCP.uplugin`

- **From:** WS-11
- **To:** WS-03 (owns `Plugins/UEREMCP/UEREMCP.uplugin`)
- **Date:** 2026-07-29
- **Blocks:** editor execution of `UEREMCP.Validation.*` automation tests

## Ask

Add an Editor module entry for `UeremcpValidation` to `UEREMCP.uplugin`, and ensure
the plugin dependency list enables `FileSandbox` (or relies on ToolsetRegistry's
existing dependency) so `FGlobalSandbox` is available at runtime.

Suggested module block (mirror existing Editor modules):

```json
{
  "Name": "UeremcpValidation",
  "Type": "Editor",
  "LoadingPhase": "Default",
  "TargetAllowList": [ "Editor" ]
}
```

Also add to `Plugins` if not already pulled transitively:

```json
{ "Name": "FileSandbox", "Enabled": true }
```

(Confirm whether ToolsetRegistry already enables FileSandbox — it declares the
dependency `[VERIFIED: ToolsetRegistry.uplugin per GROUNDED_FACTS / ADR-0005]`.
If transitive enablement works in RE, the explicit FileSandbox line is optional.)

## Why

WS-11 owns `Plugins/UEREMCP/Source/UeremcpValidation/**` and has landed:

- scratch-path helpers
- `UEREMCP.Validation.Harness.Smoke`
- `UEREMCP.Validation.Rollback.MultiAssetDiscard` (ADR-0005 gate)

Without uplugin registration the module never loads and the Wave 1 harness cannot
run inside RE. WS-11 cannot edit the uplugin (ownership).

## Response

**Accepted; assigned to WS-03.** Register `UeremcpValidation` in `UEREMCP.uplugin`
once `Plugins/UEREMCP/Source/UeremcpValidation/**` is present (merge/checkout from
`ws-11-validation` if needed). Probe plugin remains interim only.

## Interim workaround (already in tree)

Until this lands, WS-11 ships a standalone probe plugin under
`tests/integration/editor_plugin/UeremcpValidationProbe/` (junctioned into
`$PROJ/Plugins/UeremcpValidationProbe`, `EnabledByDefault: false`).
`tests/run_editor_tests.ps1` defaults to:

```text
-DisablePlugins=UEREMCP -EnablePlugins=UeremcpValidationProbe
```

so Cmd can start even when `UeremcpCore` is missing. That is a **temporary** path;
permanent registration here is still required so domain WSs run validation tests
without the probe.

Observed blocker without the workaround (2026-07-29):  
`Plugin 'UEREMCP' failed to load because module 'UeremcpCore' could not be found`
— editor aborts before Automation RunTests.

## What WS-11 will do once registered

Rebuild RE with the module, run with `-KeepUeremcp` (or drop DisablePlugins):

```powershell
pwsh tests/run_editor_tests.ps1 -Filter "UEREMCP.Validation.Harness.Smoke" -KeepUeremcp
pwsh tests/run_editor_tests.ps1 -Filter "UEREMCP.Validation.Rollback.MultiAssetDiscard" -KeepUeremcp
```

and publish runtime q1/q3 verdicts into `docs/research/RB-06-sandbox-and-rollback.md`.
