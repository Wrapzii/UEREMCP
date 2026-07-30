# WS-01: RE project plugin junction retarget (orch)

**Date:** 2026-07-30
**Status:** Restored after unauthorized retarget

## Current restore

At 2026-07-30 01:08 EDT, the RE project junction was found pointing at:

`$UEREMCP_ROOT-ws06\Plugins\UEREMCP`

The editor and Live Coding console were closed, and the junction was restored to:

`$UEREMCP_ROOT-ws01\Plugins\UEREMCP` (`ws-01-orch`)

`[VERIFIED-RUNTIME: PowerShell Get-Item -Force reported the before target as ws06,
then reported LinkType=Junction and the after target as UEREMCP-ws01 after rmdir/mklink /J]`

The orch tip was `e9bc110`; `3e8a2a3` was confirmed as an ancestor. The target
contains all nine source-module directories: `UeremcpBlueprint`, `UeremcpCore`,
`UeremcpMaterial`, `UeremcpNiagara`, `UeremcpProtocol`, `UeremcpSecurity`,
`UeremcpTemplates`, `UeremcpTransport`, and `UeremcpValidation`.
`[VERIFIED-RUNTIME: git merge-base --is-ancestor and PowerShell directory enumeration
in UEREMCP-ws01 at e9bc110]`

## Compile attempt

With the editor and Live Coding closed, an REEditor Development build was queued
with `-NoHotReloadFromIDE -WaitMutex`. After waiting on an existing `Build.bat`,
UBT started and failed quickly (exit 8 / RulesError):

```text
Invalidating makefile for REEditor (UeremcpNiagara.Build.cs modified)
Could not find definition for module 'Editor',
  (referenced via REEditor -> UeremcpMaterial.Build.cs)
Result: Failed (RulesError)
```

Junction remained on orch after the failure. This is a Material module rules issue
for the WS-08 lane / follow-up smoke, not a reason to leave the RE junction on a
workstream checkout.
`[VERIFIED-RUNTIME: Build.bat/UBT log, exit_code=8]`

## Process defect

Some workstream agent retargeted the shared RE junction to ws06 after the previous
orch restore. This is a process defect: it makes RE load a partial workstream source
tree and can reproduce phantom-module and incomplete-source failures.

The junction is shared integration infrastructure, not a per-workstream switch.

## Hard policy (integration tip)

**Workstream agents MUST NOT retarget `RE\Plugins\UEREMCP`. Only orch integration
may create, remove, or retarget this junction.** It must resolve to
`UEREMCP-ws01\Plugins\UEREMCP` on branch `ws-01-orch`.

Do not point it at ws03, ws06, ws07, ws08, or any other workstream checkout. A
workstream needing isolated compilation must build from its own path/package and
must coordinate with the orch integration lane when RE project loading is required.
