# WS-04 → WS-03/WS-11: RE editor harness blockers for Transport tests

**Date:** 2026-07-30  
**From:** WS-04  
**Needs:** `Plugins/UEREMCP/UEREMCP.uplugin` module list aligned with built DLLs; optional harness flag for Transport-only sandbox

## Observed blockers

1. **Phantom modules in RE junction uplugin (`UEREMCP-ws03`)** — `UEREMCP.uplugin` lists
   `UeremcpBlueprint`, `UeremcpNiagara`, `UeremcpSecurity`, `UeremcpTemplates` without
   corresponding `Source/` trees or `Binaries/Win64/UnrealEditor-*.dll`.  
   `pwsh tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter "UEREMCP.Transport"`
   fails at plugin load (`UeremcpSecurity could not be found`).

2. **Live Coding / open editor** — copying `UnrealEditor-UeremcpTransport.dll` into the
   RE plugin `Binaries/` fails with *file in use by another process* when the interactive
   RE editor is running. Rebuild requires `-NoHotReloadFromIDE` and/or closing the editor.

3. **WS-11 harness assumes shipping UEREMCP** — Transport tests are valid with a
   minimal sidecar plugin (`UEREMCPTransportTest` + `TransportAutomation.uproject`).
   WS-04 used this for `[VERIFIED-RUNTIME]` until the shipping uplugin is trimmed.

## Ask

- WS-03: Do not register editor modules in `UEREMCP.uplugin` until `Source/**` + DLL exist.
- WS-11 (optional): Document sidecar/sandbox path for domain modules during parallel dev.

## WS-04 workaround used

`RunUAT BuildPlugin` on a Transport-only temp plugin + minimal `.uproject` in
`%TEMP%/ueremcp-transport-automation`. Log:
`tests/integration/_logs/editor_UEREMCP_Transport_final_20260730.log`.

## Response (WS-01, 2026-07-30)

**Status:** Partially addressed on orch; re-verify on RE.

The RE plugin junction now points at **ws-01-orch** with all **eight** editor modules built and present under Binaries/Win64/ after the WS-15 Templates JSON compile fix (orch merge 9eb7531 / eea1622). Item (1) **phantom modules** reflected a **stale ws-03 tip** at UEREMCP-ws03, not current orch uplugin state.

**Recommended:** Re-run `pwsh tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter "UEREMCP.Transport"` against RE with the updated junction. Report `[VERIFIED-RUNTIME]` or remaining blockers (items 2-3) inline here.

WS-11 sidecar path remains optional for parallel domain work; not required once shipping uplugin + DLLs align on RE.