# WS-06 â†’ WS-03: register UeremcpBlueprint in UEREMCP.uplugin

**Date:** 2026-07-30  
**From:** WS-06  
**To:** WS-03  
**Status:** Accepted  
**Needs:** `Plugins/UEREMCP/UEREMCP.uplugin` module entry

## Ask

Add a `UeremcpBlueprint` editor module to `UEREMCP.uplugin` after `UeremcpValidation`
(or alongside other domain modules):

```json
{
  "Name": "UeremcpBlueprint",
  "Type": "Editor",
  "LoadingPhase": "Default",
  "TargetAllowList": [ "Editor" ]
}
```

No new plugin dependencies beyond what `UeremcpBlueprint.Build.cs` already declares
(`UeremcpProtocol`, `ToolsetRegistry`, standard editor modules).

## Why

WS-06 owns `Plugins/UEREMCP/Source/UeremcpBlueprint/**`. P0 scaffolding is complete
(toolset, envelope echo, automation tests, domain schema stubs) but cannot link or load
until the uplugin lists the module.

## Module sources (ready for wiring)

- `Plugins/UEREMCP/Source/UeremcpBlueprint/UeremcpBlueprint.Build.cs`
- `Plugins/UEREMCP/Source/UeremcpBlueprint/Public/UeremcpBlueprintToolset.h`
- `Plugins/UEREMCP/Source/UeremcpBlueprint/Private/UeremcpBlueprintModule.cpp`
- `Plugins/UEREMCP/Source/UeremcpBlueprint/Private/UeremcpBlueprintToolset.cpp`
- `Plugins/UEREMCP/Source/UeremcpBlueprint/Private/Tests/UeremcpBlueprintToolsetTests.cpp`

## Non-goals

- Do not move Blueprint logic into `UeremcpCore`.
- Do not enable Epic `BlueprintTools` as a plugin dependency here â€” composition happens
  at call time in P1+, not via uplugin coupling.

## Response (WS-03)

**Accepted** â€” WS-03 registers `UeremcpBlueprint` in uplugin.
## WS-01 follow-up (2026-07-30)

- **Status:** Still **open** — see docs/proposals/ws-01-blueprint-still-unregistered.md. Sources merged; uplugin entry pending WS-03.

