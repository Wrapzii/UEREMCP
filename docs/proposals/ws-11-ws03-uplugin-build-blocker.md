# Proposal: WS-03 `UEREMCP.uplugin` lists modules with no `Source/` tree (build + RE gate blocker)

- **From:** WS-11
- **To:** WS-03 (owns `Plugins/UEREMCP/UEREMCP.uplugin`)
- **Date:** 2026-07-30
- **Blocks:** `pwsh tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter "UEREMCP"`; rebuilding `UnrealEditor-UeremcpValidation.dll` with new ADR-0005/0006 tests

## Symptom

`RE.uproject` junctions `Plugins/UEREMCP` → `UEREMCP-ws03/Plugins/UEREMCP`.

UnrealEditor-Cmd aborts before Automation:

```text
Plugin 'UEREMCP' failed to load because module 'UeremcpBlueprint' could not be found.
```

`UnrealBuildTool REEditor ...` fails at makefile generation:

```text
Could not find definition for module 'UeremcpBlueprint', (referenced via RE.uproject -> UEREMCP.uplugin)
```

## Cause

`UEREMCP.uplugin` lists `UeremcpBlueprint` (and `UeremcpNiagara`, etc.) but
`Plugins/UEREMCP/Source/UeremcpBlueprint/` **does not exist** in `UEREMCP-ws03`
(verified 2026-07-30). `Binaries/Win64/UnrealEditor.modules` only lists Core,
Protocol, Transport, Validation — out of sync with uplugin.

## Impact on WS-11

- New tests are implemented in `UEREMCP-ws11` `UeremcpValidation/**` (SoT) and synced to ws03 junction for RE builds.
- **Cannot rebuild** `UnrealEditor-UeremcpValidation.dll` until uplugin ↔ Source parity is restored.
- Stale DLL (2026-07-29 23:40) exposes only `Harness.Smoke` + `Rollback.MultiAssetDiscard`.

## Ask

One of:

1. **Remove** uplugin module entries that have no `Source/<Module>/` tree yet, **or**
2. **Land** the missing module scaffolds (at minimum empty `IModuleInterface` stubs) so UBT can link.

After fix, WS-11 will re-run:

```powershell
pwsh tests/run_editor_tests.ps1 -KeepUeremcp -NoProbe -Filter "UEREMCP"
```

## WS-11 deliverable status

Test **source** is ready; **runtime verification** is blocked on this uplugin issue, not on test logic.
