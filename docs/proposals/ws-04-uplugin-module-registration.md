# WS-04 → WS-03: register UeremcpTransport in UEREMCP.uplugin

**Date:** 2026-07-29  
**From:** WS-04  
**Needs:** `Plugins/UEREMCP/UEREMCP.uplugin` module entry

## Ask

Add a `UeremcpTransport` editor module to `UEREMCP.uplugin` after `UeremcpProtocol`:

```json
{
  "Name": "UeremcpTransport",
  "Type": "Editor",
  "LoadingPhase": "Default",
  "TargetAllowList": [ "Editor" ]
}
```

No new plugin dependencies beyond what the module's `Build.cs` already declares
(`ModelContextProtocol`, `ModelContextProtocolEngine`).

## Why

WS-04 owns `Plugins/UEREMCP/Source/UeremcpTransport/**` — capability probe and
job-model constraints for WS-05. The code is complete but cannot link until the
uplugin lists the module.

## Non-goals

Do not move transport logic into `UeremcpCore`. This module stays a thin adapter
over Epic's public MCP API per ADR-0002.
