# WS-01 compile blocker ownership map

- **From:** WS-01 orchestration
- **To:** WS-06 Blueprint, WS-09 Gameplay, WS-15 Templates, WS-11 Validation
- **Date:** 2026-07-30
- **Status:** original WS-06/09/15 blockers closed; WS-11 full-target blocker open
- **Evidence:** `tests/integration/Editor.Handoff.Gates.md`; targeted UBT module
  rebuilds on orch `8eebca0`

## Reproduction

With the existing RE junction unchanged, WS-01 ran:

```powershell
UnrealBuildTool REEditor Win64 Development -Project=<RE.uproject> `
  -Module=<UeremcpBlueprint|UeremcpGameplay|UeremcpTemplates> `
  -WaitMutex -NoHotReloadFromIDE -NoUBA
```

No active UBT or Live Coding process held the build. Each targeted module exited
`6` (`OtherCompilationError`) before editor Automation discovery
`[VERIFIED-RUNTIME: targeted UBT module builds on 2026-07-30]`.

## Error-to-owner map

| Owner | Compiler blocker | Requested fix |
|---|---|---|
| WS-06 | `UeremcpBlueprintEpicBridge.cpp(121)`: C2668 ambiguous `EscapeJsonString`, then C2100 | Disambiguate/rename the local helper and rebuild `UeremcpBlueprint`. |
| WS-06 | `UeremcpBlueprintToolset.cpp`: C4150 deleting incomplete `FUeremcpBlueprintMutatingGate::FDispatchHolder` through `TUniquePtr` | Move destruction to a translation unit where the holder is complete, or otherwise make ownership destruction compile with the complete type. |
| WS-06 | `UeremcpBlueprintGraphWriter.cpp(210)`: C2672 using `Values.GetKeys(TArray<FString>)` with UE 5.8 shared-string JSON keys | Iterate/convert UE 5.8 JSON shared-string keys explicitly, then rebuild. |
| WS-09 | `UeremcpGameplayToolset.cpp`: C2039 for six nonexistent `FJsonObject::SetNullField` calls | Express JSON null using the verified UE 5.8 JSON surface already used elsewhere in the plugin. |
| WS-09 | `UeremcpSpellPlanner.cpp(43)`: C2665 using `Values.Find(FString)` with UE 5.8 shared-string JSON keys | Use the UE 5.8 key type or an explicit conversion before lookup. |
| WS-15 | `UeremcpTemplatesModule.cpp`: C1083, ToolsetRegistry's `UToolsetRegistry.h` cannot resolve `Kismet/BlueprintFunctionLibrary.h` | Correct `UeremcpTemplates.Build.cs` dependencies/include propagation and rebuild `UeremcpTemplates`. |

All entries above are `[VERIFIED-RUNTIME: compiler diagnostics from targeted
module builds and WS-11 commit acaad61]`.

## Resolution verification

WS-01 integrated WS-06 `9182699` as `95fdb67`, WS-09 `8d75716` as
`731a8d7`, and WS-15 `7ae7e0d` as `9ff0443`. On that tip, all six targeted
module builds returned `Result: Succeeded` and exit `0`:

- `UeremcpBlueprint`
- `UeremcpGameplay`
- `UeremcpTemplates`
- `UeremcpMaterial`
- `UeremcpNiagara`
- `UeremcpCore`

`[VERIFIED-RUNTIME: six targeted UBT module builds on 2026-07-30]`.

## Remaining full-target blocker

The subsequent full `REEditor Win64 Development` build reached the WS-11
validation tests and failed with C1083:

`BlueprintMutatingDispatchGate.spec.cpp` includes
`UeremcpBlueprintMutatingGate.h`, whose public include of
`UeremcpMutatingDispatch.h` is not visible while compiling
`UeremcpValidation` `[VERIFIED-RUNTIME: full REEditor UBT build on 2026-07-30]`.

Owner: **WS-11**, because the failing consumer and its module rules are under
`Plugins/UEREMCP/Source/UeremcpValidation/**`. Requested fix: declare the
required `UeremcpCore` dependency/include visibility in
`UeremcpValidation.Build.cs`, then rebuild `UeremcpValidation` and the full
target. WS-01 does not edit that foreign-owned build rule.

## Integration consequence

The stale `UnrealEditor.modules` omission of `UeremcpMaterial` cannot be corrected
until the full target succeeds. Editor filters remain blocked before Automation
discovery. No A6, B7, Material, Niagara, or POC B runtime pass is claimed.

After WS-11 lands its fix, rerun `UeremcpValidation`, the full `REEditor` build,
then `pwsh tests/run_editor_handoff_gates.ps1 -Gate All`.
