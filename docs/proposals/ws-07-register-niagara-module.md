# WS-07 → WS-03: Register UeremcpNiagara in UEREMCP.uplugin

- **From:** WS-07
- **To:** WS-03
- **Date:** 2026-07-30
- **Status:** requested (Wave 2 scaffold landed)

## Ask

Add the `UeremcpNiagara` editor module to `Plugins/UEREMCP/UEREMCP.uplugin` and ensure
the target project enables **NiagaraToolsets** (and Niagara editor modules) so
`UeremcpNiagara.Build.cs` private dependencies resolve.

### Proposed `.uplugin` module entry

```json
{
  "Name": "UeremcpNiagara",
  "Type": "Editor",
  "LoadingPhase": "Default",
  "TargetAllowList": ["Editor"]
}
```

Insert after `UeremcpProtocol` (domain modules load after core/protocol).

### Plugin dependencies

Confirm or add in `Plugins` array:

```json
{ "Name": "Niagara", "Enabled": true }
```

NiagaraToolsets is typically pulled by the RE project when MCP Niagara tools are used;
if compile fails on missing `NiagaraToolsets`, add explicit plugin dependency.

## What WS-07 shipped

- `Plugins/UEREMCP/Source/UeremcpNiagara/**` — module + `UUeremcpNiagaraToolset`
  (`Echo`, `InspectSystem` stub)
- `schemas/domains/niagara/**` — `inspect_system`, `create_niagara_effect`, `graph-ext`

## Registration

`UUeremcpNiagaraToolset` follows the same `UToolsetDefinition` + `AICallable` pattern as
`UUeremcpReferenceToolset`. Resolve RB-03 q4 (auto-discovery vs explicit
`RegisterToolset`) when wiring domain toolsets.

Apply `SetNameFilters` to hide internal NiagaraToolsets primitives once batching is live
(ADR-0002 rule 5).

## Blocker

Until this lands, the Niagara module **will not compile in the RE project** when copied
into `Plugins/UEREMCP` — Unreal only builds modules listed in `.uplugin`.
