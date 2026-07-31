# WS-01 → WS-03: register UeremcpSystems module in UEREMCP.uplugin

This branch adds `Plugins/UEREMCP/Source/UeremcpSystems/**` (WS-01 owned). The
`.uplugin` Modules array is owned by WS-03. The module entry was added on this
branch so BuildPlugin can compile; please confirm / adopt:

```json
{
  "Name": "UeremcpSystems",
  "Type": "Editor",
  "LoadingPhase": "Default",
  "TargetAllowList": [ "Editor" ]
}
```

No other Core transport changes requested.
