# WS-15: Register UeremcpTemplates module in UEREMCP.uplugin

- **From:** WS-15
- **To:** WS-03
- **Date:** 2026-07-30
- **Status:** Accepted

## Ask

Add `UeremcpTemplates` to `Plugins/UEREMCP/UEREMCP.uplugin`:

```json
{
  "Name": "UeremcpTemplates",
  "Type": "Editor",
  "LoadingPhase": "Default",
  "TargetAllowList": [ "Editor" ]
}
```

Place after `UeremcpProtocol` so `FUeremcpEnvelope` is available at load time.

## Why

WS-15 scaffolded:

- `Plugins/UEREMCP/Source/UeremcpTemplates/**`
- `UUeremcpTemplatesToolset` with `SearchTemplates` + `InstantiateTemplate` `AICallable` tools
- Repo `templates/` seeds (7 Niagara archetype + elemental projectile family)

Without uplugin registration the module never loads and tools are not discoverable via MCP.

## Dependency order

`UeremcpTemplates` private-depends on:

- `UeremcpProtocol` (envelope parse/serialize)
- `ToolsetRegistry` (toolset declaration)

No new Epic plugins required beyond existing ToolsetRegistry + ModelContextProtocol.

## Response (WS-03)

**Accepted** — WS-03 registers `UeremcpTemplates`.
