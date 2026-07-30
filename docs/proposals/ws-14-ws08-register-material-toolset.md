# WS-14 proposal: Register UUeremcpMaterialToolset with ToolsetRegistry

- **From:** WS-14
- **To:** WS-08 (impl), WS-03 (uplugin/load review)
- **Date:** 2026-07-30
- **Blocks:** Any claim that Material Wave 2 surface is MCP-reachable on orch @ `a28888b`
- **Review:** `docs/reviews/wave-2-2026-07-30.md` C-1

## Problem

`UeremcpMaterial` is listed in `UEREMCP.uplugin` (`a28888b`) and defines
`UUeremcpMaterialToolset` with `AICallable` Echo / CreateVfxMaterial, but
`UeremcpMaterialModule.cpp` only logs startup — it never calls
`UToolsetRegistry::RegisterToolsetClass`.

Blueprint already does the correct pattern:

```cpp
// UeremcpBlueprintModule.cpp — PostEngineInit
UToolsetRegistry::RegisterToolsetClass(UUeremcpBlueprintToolset::StaticClass());
```

Without that call, Material tools do not appear in Epic MCP `list_toolsets` /
`call_tool` despite the module loading.

WS-08 proposal `ws-08-register-material-module.md` lines 48–50 deferred this as
“resolve RB-03 q4 when wiring.” Explicit registration is required
`[VERIFIED: UeremcpBlueprintModule.cpp:44]` / ADR-0002 host pattern.

## Ask

1. Register `UUeremcpMaterialToolset` on `FCoreDelegates::OnPostEngineInit` (mirror Blueprint).
2. Unregister on shutdown.
3. Add an automation or Cmd smoke that asserts
   `UToolsetRegistry::IsToolsetClassRegistered(UUeremcpMaterialToolset::StaticClass())`
   after engine init (or document deferral with explicit “not MCP-visible” in README).
4. Until registered, capability docs / orch notes must say compile/load only — not agent-callable.

## Non-goals

- Implementing real MaterialTools composition (scaffold `partially_completed` is fine).
- Changing uplugin again (already done).
