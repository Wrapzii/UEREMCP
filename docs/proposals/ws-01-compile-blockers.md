# WS-01 compile blocker ownership map

- **From:** WS-01 orchestration
- **To:** WS-06 Blueprint, WS-09 Gameplay, WS-15 Templates
- **Date:** 2026-07-30
- **Status:** open
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

## Integration consequence

The stale `UnrealEditor.modules` omission of `UeremcpMaterial` cannot be corrected
by a module-only link while the full target still fails. Blueprint, Material,
Niagara, and B7 editor filters remain blocked before Automation discovery. No A6,
B7, Material, Niagara, or POC B runtime pass is claimed.

WS-01 owns none of the failing source/build-rule paths, so this proposal records
the blocker without modifying foreign workstreams. After WS-06, WS-09, and WS-15
land fixes, rerun the three module builds, then the full `REEditor` build, then
`pwsh tests/run_editor_handoff_gates.ps1 -Gate All`.
