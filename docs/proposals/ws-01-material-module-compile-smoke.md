# WS-01 Material module compile smoke on RE

**Date:** 2026-07-30  
**Branch:** `ws-01-orch` at `a28888b`  
**Result:** `partially_completed` — prerequisite and contention evidence captured;
compile and registration smoke not proven.

## Preconditions observed

- The orchestration checkout is at `a28888b`, and `f296cec` is an ancestor.
  `[VERIFIED-RUNTIME: git log and git merge-base --is-ancestor on UEREMCP-ws01]`
- `UEREMCP.uplugin` declares nine modules, including `UeremcpMaterial`, and declares
  the `EditorToolset` plugin dependency.
  `[VERIFIED-RUNTIME: read UEREMCP-ws01/Plugins/UEREMCP/UEREMCP.uplugin at a28888b]`
- The required RE junction precondition was **not** met. At 2026-07-30 01:05 EDT,
  `RE/Plugins/UEREMCP` was a junction to
  `$UEREMCP_ROOT-ws06\Plugins\UEREMCP`, not
  `UEREMCP-ws01`.
  `[VERIFIED-RUNTIME: PowerShell Get-Item -Force reported LinkType=Junction and the ws06 Target]`

The junction was not changed by this verification run.

## Offline compile attempts

An RE-targeted foreign-plugin compile was attempted without changing the junction:

```text
Build.bat UnrealEditor Win64 Development
  -Project="...\RE\RE.uproject"
  -Plugin="...\UEREMCP-ws01\Plugins\UEREMCP\UEREMCP.uplugin"
  -BuildPluginAsLocal -Module=UeremcpMaterial
  -NoHotReloadFromIDE -WaitMutex
```

UBT exited 6 before compilation:

```text
Plugin 'UEREMCP' ... does not contain the 'UeremcpProtocol' module,
but lists it in '...\UEREMCP-ws01\Plugins\UEREMCP\UEREMCP.uplugin'.
Result: Failed (OtherCompilationError)
```

This is not compile proof. The RE project already exposed a different same-name
`UEREMCP` plugin through the ws06 junction, so the foreign-plugin attempt could not
establish the ws01 module set.
`[VERIFIED-RUNTIME: Build.bat exit 6 and captured UBT output]`

An isolated `UnrealEditor` foreign-plugin attempt also failed before C++ compilation
with `Could not find definition for module 'UeremcpProtocol'` (UBT exit 8).
`[VERIFIED-RUNTIME: Build.bat exit 8 and captured UBT output]`

A standard `RunUAT BuildPlugin` fallback was then attempted with a unique package
directory. AutomationTool refused to start because another AutomationTool instance
was active:

```text
A conflicting instance of AutomationTool is already running.
```

Process inspection identified that instance as a WS-12 `BuildPlugin` run and also
found an active RE `REEditor ... -LiveCoding` UBT process.
`[VERIFIED-RUNTIME: RunUAT exit 1 plus Win32_Process command lines for WS-12 BuildPlugin and RE LiveCoding build]`

## Load and registration smoke

Not run. Loading RE while its plugin junction resolves to ws06 would not prove that
the module registered from the ws01 orchestration tip. The active RE Live Coding
build and global WS-12 AutomationTool run also made the editor/build harness
contended. No editor process or lock was terminated, and no asset was created.

## Required follow-up

After the owning run restores the required junction to `UEREMCP-ws01` and the active
Live Coding/AutomationTool work finishes:

1. build `UeremcpMaterial` or the UEREMCP plugin for `RE.uproject` with
   `-NoHotReloadFromIDE`;
2. launch/load RE from those binaries;
3. verify the Material toolset through toolset list/describe and a non-destructive
   echo call if exposed;
4. record the successful build and runtime output here.

