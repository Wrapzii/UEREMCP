# WS-04 → WS-03/WS-11: RE editor harness blockers for Transport tests

**Date:** 2026-07-30  
**From:** WS-04  
**Status:** **Resolved** (orch junction + 8-module uplugin)

## Original blockers (2026-07-30 AM)

1. **Phantom modules in RE junction uplugin (`UEREMCP-ws03`)** — `UEREMCP.uplugin` listed
   modules without `Source/` trees or DLLs. Plugin load failed (`UeremcpSecurity could not
   be found`).

2. **Live Coding / open editor** — DLL copy failed when interactive RE editor held locks.
   Rebuild requires `-NoHotReloadFromIDE` and/or closing the editor.

3. **WS-11 harness assumes shipping UEREMCP** — Transport tests were run via sidecar
   sandbox until the shipping uplugin was complete.

## Resolution

- RE junction retargeted to `UEREMCP-ws01\Plugins\UEREMCP` (`ws-01-orch`) with all eight
  editor modules present and compiling. See `docs/proposals/ws-01-re-junction-orch.md`.
- WS-03 phantom-module issue is **stale** on the orch tip; no uplugin trim required.

## Response — RE shipping harness results `[VERIFIED-RUNTIME: 2026-07-30]`

**Command:**

```powershell
pwsh tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter "UEREMCP.Transport"
```

**Project:** `RE.uproject` (junction → `UEREMCP-ws01\Plugins\UEREMCP`)  
**Build:** `Build.bat REEditor … -Module=UeremcpTransport -NoHotReloadFromIDE` — Succeeded  
**Exit code:** 0  
**Log:** `tests/integration/_logs/editor_UEREMCP_Transport_20260730_010212.log`

| Test | Path | Result |
|---|---|---|
| Handoff drift guard | `UEREMCP.Transport.Handoff.DriftGuard` | **PASS** |
| Dispatch model | `UEREMCP.Transport.DispatchModel` | **PASS** |
| Job state invariants | `UEREMCP.Transport.JobState.Invariants` | **PASS** |
| Job state negative | `UEREMCP.Transport.JobState.Negative` | **PASS** |
| Epic MCP probe | `UEREMCP.Transport.Probe.EpicMcp` | **PASS** |
| Job registry poll | `UEREMCP.Transport.JobRegistry.Poll` | **SKIP** (registry not implemented) |
| Job registry cancel | `UEREMCP.Transport.JobRegistry.Cancel` | **SKIP** (registry not implemented) |
| Timeout partially completed | `UEREMCP.Transport.Timeout.PartiallyCompleted` | **SKIP** (registry not implemented) |

**Summary:** 8/8 automation `Success` — **5 PASS + 3 SKIP** (expected; JobRegistry deferred to ADR-0009 / WS-05).

## Historical workaround (pre-orch)

`RunUAT BuildPlugin` on a Transport-only temp plugin + minimal `.uproject` in
`%TEMP%/ueremcp-transport-automation`. Log:
`tests/integration/_logs/editor_UEREMCP_Transport_final_20260730.log`.

## Response (WS-01, 2026-07-30)

**Status:** Partially addressed on orch; re-verify on RE.

The RE plugin junction now points at **ws-01-orch** with all **eight** editor modules built and present under Binaries/Win64/ after the WS-15 Templates JSON compile fix (orch merge 9eb7531 / eea1622). Item (1) **phantom modules** reflected a **stale ws-03 tip** at UEREMCP-ws03, not current orch uplugin state.

**Recommended:** Re-run `pwsh tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter "UEREMCP.Transport"` against RE with the updated junction. Report `[VERIFIED-RUNTIME]` or remaining blockers (items 2-3) inline here.

WS-11 sidecar path remains optional for parallel domain work; not required once shipping uplugin + DLLs align on RE.